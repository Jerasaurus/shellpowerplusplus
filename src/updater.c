/*
 * Auto-updater implementation using libcurl
 */

#include "updater.h"
#include "version.h"
#include "lib/tinyfiledialogs.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <process.h>
#define PATH_SEP "\\"
#else
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <pthread.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#define PATH_SEP "/"
extern char **environ;
#endif

// Global state for async update check
static UpdateCheckResult g_async_result = {0};
static volatile bool g_check_in_progress = false;
static volatile bool g_check_complete = false;

#ifdef _WIN32
static HANDLE g_update_thread = NULL;
#else
static pthread_t g_update_thread;
static bool g_thread_created = false;
#endif

// GitHub API URL (constructed from version.h defines)
#define GITHUB_API_URL_FMT "https://api.github.com/repos/%s/%s/releases/latest"

//------------------------------------------------------------------------------
// CURL Response Buffer
//------------------------------------------------------------------------------
typedef struct {
    char *data;
    size_t size;
} CurlBuffer;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    CurlBuffer *buf = (CurlBuffer *)userp;

    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr)
        return 0;

    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

// Write callback for file download
static size_t WriteFileCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    FILE *fp = (FILE *)userp;
    return fwrite(contents, size, nmemb, fp);
}

//------------------------------------------------------------------------------
// Simple JSON Value Extraction (no external library needed)
//------------------------------------------------------------------------------
static bool ExtractJSONString(const char *json, const char *key, char *value, size_t value_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *key_pos = strstr(json, search);
    if (!key_pos)
        return false;

    const char *colon = strchr(key_pos + strlen(search), ':');
    if (!colon)
        return false;

    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    if (*p != '"')
        return false;
    p++;

    size_t i = 0;
    while (*p && *p != '"' && i < value_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            if (*p == 'n')
                value[i++] = '\n';
            else if (*p == 't')
                value[i++] = '\t';
            else if (*p == 'r')
                value[i++] = '\r';
            else
                value[i++] = *p;
        } else {
            value[i++] = *p;
        }
        p++;
    }
    value[i] = '\0';
    return true;
}

//------------------------------------------------------------------------------
// Version Comparison
//------------------------------------------------------------------------------
int CompareVersions(const char *version_a, const char *version_b) {
    if (*version_a == 'v')
        version_a++;
    if (*version_b == 'v')
        version_b++;

    int major_a = 0, minor_a = 0, patch_a = 0;
    int major_b = 0, minor_b = 0, patch_b = 0;

    sscanf(version_a, "%d.%d.%d", &major_a, &minor_a, &patch_a);
    sscanf(version_b, "%d.%d.%d", &major_b, &minor_b, &patch_b);

    if (major_a != major_b)
        return (major_a < major_b) ? -1 : 1;
    if (minor_a != minor_b)
        return (minor_a < minor_b) ? -1 : 1;
    if (patch_a != patch_b)
        return (patch_a < patch_b) ? -1 : 1;
    return 0;
}

//------------------------------------------------------------------------------
// Platform-Specific Functions
//------------------------------------------------------------------------------
bool GetExecutablePath(char *path_out, size_t path_size) {
#ifdef _WIN32
    DWORD len = GetModuleFileNameA(NULL, path_out, (DWORD)path_size);
    return len > 0 && len < path_size;
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)path_size;
    return _NSGetExecutablePath(path_out, &size) == 0;
#else
    ssize_t len = readlink("/proc/self/exe", path_out, path_size - 1);
    if (len > 0) {
        path_out[len] = '\0';
        return true;
    }
    return false;
#endif
}

bool OpenBrowserURL(const char *url) {
#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    return (intptr_t)result > 32;
#elif defined(__APPLE__)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open \"%s\"", url);
    return system(cmd) == 0;
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" &", url);
    return system(cmd) == 0;
#endif
}

static bool GetTempFilePath(char *path_out, size_t path_size, const char *filename) {
#ifdef _WIN32
    char temp_dir[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_dir) == 0)
        return false;
    snprintf(path_out, path_size, "%s%s", temp_dir, filename);
    return true;
#else
    const char *temp_dir = getenv("TMPDIR");
    if (!temp_dir)
        temp_dir = "/tmp";
    snprintf(path_out, path_size, "%s/%s", temp_dir, filename);
    return true;
#endif
}

//------------------------------------------------------------------------------
// Download Functions
//------------------------------------------------------------------------------
static bool DownloadFile(const char *url, const char *output_path) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return false;

    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "shellpower-updater/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // 2 minute timeout for download

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        remove(output_path);
        curl_easy_cleanup(curl);
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        remove(output_path);
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Self-Update Functions
//------------------------------------------------------------------------------
bool DownloadAndInstallUpdate(const UpdateCheckResult *result) {
    if (!result || !result->update_available)
        return false;

    char exe_path[512];
    if (!GetExecutablePath(exe_path, sizeof(exe_path))) {
        tinyfd_messageBox("Update Error", "Could not determine executable path.", "ok", "error", 1);
        return false;
    }

    // Create temp file path for download
    char temp_path[512];
#ifdef _WIN32
    if (!GetTempFilePath(temp_path, sizeof(temp_path), "shellpower_update.exe")) {
#else
    if (!GetTempFilePath(temp_path, sizeof(temp_path), "shellpower_update")) {
#endif
        tinyfd_messageBox("Update Error", "Could not create temporary file path.", "ok", "error", 1);
        return false;
    }

    // Show progress message
    tinyfd_messageBox("Downloading Update",
        "Downloading update... This may take a moment.\nClick OK to start.",
        "ok", "info", 1);

    // Download the new binary
    if (!DownloadFile(result->download_url, temp_path)) {
        tinyfd_messageBox("Update Error", "Failed to download update.", "ok", "error", 1);
        return false;
    }

    // Make the downloaded file executable (Unix only)
#ifndef _WIN32
    chmod(temp_path, 0755);
#endif

    // Create backup path for old executable
    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s.old", exe_path);

    // Remove any existing backup
    remove(backup_path);

#ifdef _WIN32
    // On Windows, we can't replace a running executable directly
    // Create a batch script to do the replacement after exit
    char batch_path[512];
    if (!GetTempFilePath(batch_path, sizeof(batch_path), "shellpower_update.bat")) {
        tinyfd_messageBox("Update Error", "Could not create update script.", "ok", "error", 1);
        remove(temp_path);
        return false;
    }

    FILE *batch = fopen(batch_path, "w");
    if (!batch) {
        tinyfd_messageBox("Update Error", "Could not write update script.", "ok", "error", 1);
        remove(temp_path);
        return false;
    }

    fprintf(batch,
        "@echo off\n"
        "echo Waiting for application to close...\n"
        "timeout /t 2 /nobreak >nul\n"
        ":retry\n"
        "move /y \"%s\" \"%s\" >nul 2>&1\n"
        "if errorlevel 1 (\n"
        "    timeout /t 1 /nobreak >nul\n"
        "    goto retry\n"
        ")\n"
        "move /y \"%s\" \"%s\"\n"
        "if errorlevel 1 (\n"
        "    echo Update failed!\n"
        "    move /y \"%s\" \"%s\"\n"
        "    pause\n"
        "    exit /b 1\n"
        ")\n"
        "echo Update complete! Starting application...\n"
        "start \"\" \"%s\"\n"
        "del \"%%~f0\"\n",
        exe_path, backup_path,
        temp_path, exe_path,
        backup_path, exe_path,
        exe_path);
    fclose(batch);

    // Launch the batch script and exit
    ShellExecuteA(NULL, "open", batch_path, NULL, NULL, SW_HIDE);

    return true; // Signal that we should exit
#else
    // On Unix, rename current executable to backup
    if (rename(exe_path, backup_path) != 0) {
        tinyfd_messageBox("Update Error", "Could not backup current executable.", "ok", "error", 1);
        remove(temp_path);
        return false;
    }

    // Move new executable to current path
    if (rename(temp_path, exe_path) != 0) {
        // Try to restore backup
        rename(backup_path, exe_path);
        tinyfd_messageBox("Update Error", "Could not install update.", "ok", "error", 1);
        return false;
    }

    // Clean up backup
    remove(backup_path);

    // Relaunch the application
    tinyfd_messageBox("Update Complete",
        "Update installed successfully!\nThe application will now restart.",
        "ok", "info", 1);

    // Fork and exec the new binary
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - exec the new binary
        char *args[] = {exe_path, NULL};
        execv(exe_path, args);
        _exit(1); // If exec fails
    }

    return true; // Signal that we should exit
#endif
}

//------------------------------------------------------------------------------
// Update Check Implementation
//------------------------------------------------------------------------------
void UpdaterInit(void) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void UpdaterCleanup(void) {
#ifdef _WIN32
    if (g_update_thread) {
        WaitForSingleObject(g_update_thread, 5000);
        CloseHandle(g_update_thread);
        g_update_thread = NULL;
    }
#else
    if (g_thread_created && g_check_in_progress) {
        pthread_join(g_update_thread, NULL);
        g_thread_created = false;
    }
#endif
    curl_global_cleanup();
}

//------------------------------------------------------------------------------
// Async Update Check Thread
//------------------------------------------------------------------------------
#ifdef _WIN32
static unsigned __stdcall AsyncUpdateCheckThread(void *arg) {
    (void)arg;
    g_async_result = CheckForUpdates();
    g_async_result.check_complete = true;
    g_check_in_progress = false;
    g_check_complete = true;
    return 0;
}
#else
static void *AsyncUpdateCheckThread(void *arg) {
    (void)arg;
    g_async_result = CheckForUpdates();
    g_async_result.check_complete = true;
    g_check_in_progress = false;
    g_check_complete = true;
    return NULL;
}
#endif

void StartAsyncUpdateCheck(void) {
    if (g_check_in_progress || g_check_complete)
        return;

    g_check_in_progress = true;
    g_check_complete = false;
    memset(&g_async_result, 0, sizeof(g_async_result));
    g_async_result.check_in_progress = true;

#ifdef _WIN32
    g_update_thread = (HANDLE)_beginthreadex(NULL, 0, AsyncUpdateCheckThread, NULL, 0, NULL);
#else
    if (pthread_create(&g_update_thread, NULL, AsyncUpdateCheckThread, NULL) == 0) {
        g_thread_created = true;
        pthread_detach(g_update_thread); // Don't need to join
    } else {
        g_check_in_progress = false;
    }
#endif
}

bool IsUpdateCheckComplete(void) {
    return g_check_complete;
}

UpdateCheckResult GetUpdateCheckResult(void) {
    return g_async_result;
}

UpdateCheckResult CheckForUpdates(void) {
    UpdateCheckResult result = {0};
    CURL *curl = curl_easy_init();

    if (!curl) {
        result.check_failed = true;
        strncpy(result.error_message, "Failed to initialize CURL", sizeof(result.error_message) - 1);
        return result;
    }

    CurlBuffer buffer = {0};
    buffer.data = malloc(1);
    buffer.size = 0;

    char api_url[256];
    snprintf(api_url, sizeof(api_url), GITHUB_API_URL_FMT, SHELLPOWER_GITHUB_OWNER, SHELLPOWER_GITHUB_REPO);

    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "shellpower-updater/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        result.check_failed = true;
        snprintf(result.error_message, sizeof(result.error_message),
                 "Network error: %s", curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            char tag_name[32] = {0};
            char body[1024] = {0};
            char html_url[256] = {0};

            ExtractJSONString(buffer.data, "tag_name", tag_name, sizeof(tag_name));
            ExtractJSONString(buffer.data, "body", body, sizeof(body));
            ExtractJSONString(buffer.data, "html_url", html_url, sizeof(html_url));

            strncpy(result.latest_version, tag_name, sizeof(result.latest_version) - 1);
            strncpy(result.release_notes, body, sizeof(result.release_notes) - 1);
            strncpy(result.release_url, html_url, sizeof(result.release_url) - 1);

#ifdef _WIN32
            snprintf(result.download_url, sizeof(result.download_url),
                     "https://github.com/%s/%s/releases/download/%s/shellpower-%s-%s.exe",
                     SHELLPOWER_GITHUB_OWNER, SHELLPOWER_GITHUB_REPO, tag_name,
                     SHELLPOWER_PLATFORM, SHELLPOWER_ARCH);
#else
            snprintf(result.download_url, sizeof(result.download_url),
                     "https://github.com/%s/%s/releases/download/%s/shellpower-%s-%s",
                     SHELLPOWER_GITHUB_OWNER, SHELLPOWER_GITHUB_REPO, tag_name,
                     SHELLPOWER_PLATFORM, SHELLPOWER_ARCH);
#endif

            if (strlen(tag_name) > 0) {
                result.update_available = (CompareVersions(SHELLPOWER_VERSION, tag_name) < 0);
            }
        } else {
            result.check_failed = true;
            snprintf(result.error_message, sizeof(result.error_message),
                     "GitHub API error (HTTP %ld)", http_code);
        }
    }

    free(buffer.data);
    curl_easy_cleanup(curl);

    return result;
}

bool ShowUpdateDialog(const UpdateCheckResult *result) {
    if (!result || !result->update_available)
        return false;

    char message[1024];
    snprintf(message, sizeof(message),
             "A new version of Solar Array Designer is available!\n\n"
             "Current version: %s\n"
             "Latest version: %s\n\n"
             "Would you like to download and install the update now?",
             SHELLPOWER_VERSION, result->latest_version);

    int response = tinyfd_messageBox(
        "Update Available",
        message,
        "yesno",
        "question",
        1
    );

    return response == 1;
}

const char *GetCurrentVersion(void) {
    return SHELLPOWER_VERSION;
}
