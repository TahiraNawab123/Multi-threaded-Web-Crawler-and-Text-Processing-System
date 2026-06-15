#include "downloader.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* ── libcurl write callback ─────────────────────────────── */
typedef struct { char *data; size_t size; } CurlBuf;

static size_t write_cb(void *ptr, size_t sz, size_t nmemb, void *ud)
{
    size_t   bytes = sz * nmemb;
    CurlBuf *b     = (CurlBuf *)ud;
    char    *tmp   = realloc(b->data, b->size + bytes + 1);
    if (!tmp) return 0;
    b->data = tmp;
    memcpy(b->data + b->size, ptr, bytes);
    b->size += bytes;
    b->data[b->size] = '\0';
    return bytes;
}

/* ── Download one URL, return heap string (caller frees) ── */
static char *fetch_url(const char *url)
{
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;
    CurlBuf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        60L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "OS-Crawler/1.0");
    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) {
        fprintf(stderr, "[downloader] curl: %s\n", curl_easy_strerror(rc));
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

/* ── Last-downloader detection ──────────────────────────── */
static int             downloaders_done  = 0;
static int             total_downloaders = 0;
static pthread_mutex_t done_lock         = PTHREAD_MUTEX_INITIALIZER;

void downloader_init(int num_downloaders)
{
    downloaders_done  = 0;
    total_downloaders = num_downloaders;
}

/* ── Stage 1: URL Reader ─────────────────────────────────── */
void *url_reader_thread(void *arg)
{
    URLReaderArgs *a = (URLReaderArgs *)arg;
    FILE *f = fopen(a->url_file, "r");
    if (!f) { perror("fopen"); exit(EXIT_FAILURE); }

    char line[MAX_URL_LEN];
    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r'||line[len-1]==' '))
            line[--len] = '\0';
        if (len == 0) continue;
        UrlItem *item = malloc(sizeof(UrlItem));
        strncpy(item->url, line, MAX_URL_LEN - 1);
        item->url[MAX_URL_LEN - 1] = '\0';
        queue_push(a->url_queue, item);
        printf("[url_reader] queued: %s\n", item->url);
    }
    fclose(f);
    queue_close(a->url_queue);
    printf("[url_reader] done.\n");
    return NULL;
}

/* ── Stage 2: Downloader ─────────────────────────────────── */
void *downloader_thread(void *arg)
{
    DownloaderArgs *a = (DownloaderArgs *)arg;
    double t_start    = now_sec();

    while (1) {
        UrlItem *item = (UrlItem *)queue_pop(a->url_queue);
        if (!item) break;
        printf("[downloader %lu] %s\n",
               (unsigned long)pthread_self(), item->url);

        char *text = fetch_url(item->url);
        free(item);
        if (!text) continue;

        /* Split text into chunks of chunk_size words */
        char  *saveptr = NULL;
        char  *tok     = strtok_r(text, " \t\r\n", &saveptr);
        int    wc      = 0;
        size_t cap     = 4096;
        char  *cbuf    = malloc(cap);
        size_t used    = 0;
        cbuf[0] = '\0';

        while (tok) {
            size_t tlen = strlen(tok);
            while (used + tlen + 2 > cap) { cap *= 2; cbuf = realloc(cbuf, cap); }
            if (used > 0) cbuf[used++] = ' ';
            memcpy(cbuf + used, tok, tlen);
            used += tlen;
            cbuf[used] = '\0';
            wc++;

            if (wc == a->chunk_size) {
                ChunkItem *ci = malloc(sizeof(ChunkItem));
                ci->text      = strdup(cbuf);
                ci->word_count = wc;
                queue_push(a->chunk_queue, ci);
                used = 0; cbuf[0] = '\0'; wc = 0;
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr);
        }
        /* Flush partial trailing chunk */
        if (wc > 0) {
            ChunkItem *ci = malloc(sizeof(ChunkItem));
            ci->text      = strdup(cbuf);
            ci->word_count = wc;
            queue_push(a->chunk_queue, ci);
        }
        free(cbuf);
        free(text);
    }

    double elapsed = now_sec() - t_start;
    pthread_mutex_lock(a->dl_time_lock);
    *a->dl_time_acc += elapsed;
    pthread_mutex_unlock(a->dl_time_lock);

    /* Close chunk queue only after the last downloader finishes */
    pthread_mutex_lock(&done_lock);
    downloaders_done++;
    if (downloaders_done == total_downloaders) {
        queue_close(a->chunk_queue);
        printf("[downloader] all done — chunk queue closed.\n");
    }
    pthread_mutex_unlock(&done_lock);
    return NULL;
}
