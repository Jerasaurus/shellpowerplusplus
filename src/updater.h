#ifndef UPDATER_H
#define UPDATER_H

#include <stdbool.h>
#include <stddef.h>

//------------------------------------------------------------------------------
// Update Check Result
//------------------------------------------------------------------------------
typedef struct {
    bool update_available;
    bool check_failed;
    bool check_in_progress;
    bool check_complete;
    char latest_version[32];
    char release_notes[1024];
    char release_url[256];
    char download_url[256];
    char error_message[256];
} UpdateCheckResult;

//------------------------------------------------------------------------------
// Function Declarations
//------------------------------------------------------------------------------

// Initialize the updater (call once at startup)
void UpdaterInit(void);

// Cleanup the updater (call at shutdown)
void UpdaterCleanup(void);

// Start async update check (non-blocking)
void StartAsyncUpdateCheck(void);

// Check if async update check is complete
bool IsUpdateCheckComplete(void);

// Get the result of the async update check (only valid after IsUpdateCheckComplete returns true)
UpdateCheckResult GetUpdateCheckResult(void);

// Check for updates (blocking call)
UpdateCheckResult CheckForUpdates(void);

// Show update dialog to user
// Returns true if user wants to update
bool ShowUpdateDialog(const UpdateCheckResult *result);

// Download and install update, returns true if app should exit
bool DownloadAndInstallUpdate(const UpdateCheckResult *result);

// Open URL in system browser
bool OpenBrowserURL(const char *url);

// Compare version strings (returns: -1 if a<b, 0 if a==b, 1 if a>b)
int CompareVersions(const char *version_a, const char *version_b);

// Get current version string
const char *GetCurrentVersion(void);

// Get path to current executable
bool GetExecutablePath(char *path_out, size_t path_size);

#endif // UPDATER_H
