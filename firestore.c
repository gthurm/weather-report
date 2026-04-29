#include "firestore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>

struct firestore {
    char url[512];
};

firestore_t *firestore_open(void)
{
    const char *project    = getenv("FIRESTORE_PROJECT_ID");
    const char *api_key    = getenv("FIRESTORE_API_KEY");
    const char *collection = getenv("FIRESTORE_COLLECTION");

    if (!project || !api_key) {
        fprintf(stderr, "firestore: FIRESTORE_PROJECT_ID and FIRESTORE_API_KEY must be set\n");
        return NULL;
    }
    if (!collection)
        collection = "samples";

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "firestore: curl_global_init failed\n");
        return NULL;
    }

    firestore_t *fs = malloc(sizeof(*fs));
    if (!fs) return NULL;

    snprintf(fs->url, sizeof(fs->url),
             "https://firestore.googleapis.com/v1/projects/%s/databases/(default)/documents/%s?key=%s",
             project, collection, api_key);

    return fs;
}

void firestore_close(firestore_t *fs)
{
    curl_global_cleanup();
    free(fs);
}

static void append_double(char *buf, size_t size, const char *name, const double *v, int comma)
{
    if (v)
        snprintf(buf + strlen(buf), size - strlen(buf),
                 "\"%s\":{\"doubleValue\":%.6f}%s", name, *v, comma ? "," : "");
    else
        snprintf(buf + strlen(buf), size - strlen(buf),
                 "\"%s\":{\"nullValue\":null}%s", name, comma ? "," : "");
}

/* libcurl write callback — discard response body */
static size_t discard(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr; (void)userdata;
    return size * nmemb;
}

int firestore_insert(firestore_t *fs,
                     const double *bmp_temp_c, const double *bmp_press_hpa,
                     const double *aht_temp_c, const double *aht_hum_pct)
{
    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    char body[1024] = {0};
    snprintf(body, sizeof(body),
             "{\"fields\":{"
             "\"timestamp\":{\"stringValue\":\"%s\"},", ts);
    append_double(body, sizeof(body), "bmp_temp_c",    bmp_temp_c,    1);
    append_double(body, sizeof(body), "bmp_press_hpa", bmp_press_hpa, 1);
    append_double(body, sizeof(body), "aht_temp_c",    aht_temp_c,    1);
    append_double(body, sizeof(body), "aht_hum_pct",   aht_hum_pct,   0);
    snprintf(body + strlen(body), sizeof(body) - strlen(body), "}}");

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "firestore: curl_easy_init failed\n");
        return -1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,            fs->url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  discard);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    int ok = 0;
    if (res != CURLE_OK) {
        fprintf(stderr, "firestore: %s\n", curl_easy_strerror(res));
        ok = -1;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200) {
            fprintf(stderr, "firestore: HTTP %ld\n", http_code);
            ok = -1;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}
