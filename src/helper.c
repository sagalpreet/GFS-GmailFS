#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <fuse.h>
#include <errno.h>
#include "helper.h"

// sets the username and password for curl utility
#define setCred(curl) \
    /* Set username and password */\
    curl_easy_setopt(curl, CURLOPT_USERNAME, EMAIL_);\
    curl_easy_setopt(curl, CURLOPT_PASSWORD, PWD_);

// creates a static string with name realpath that contains the complete path
#define absoluteURL(path)\
    char realpath[strlen(path) + strlen(ROOT) + 1];\
    strcpy(realpath, ROOT);\
    realpath[strlen(ROOT)] = 0;\
    strcat(realpath, path);\
    realpath[strlen(ROOT)+strlen(path)] = 0;

// creates a static string uid that contains the uid as a string
#define getUID(searchOutput)\
    char uid[strlen(searchOutput)-10];\
    strncpy(uid, searchOutput+9, strlen(searchOutput)-11);\
    uid[strlen(searchOutput)-11] = 0;

// get url with uid of mail
#define getMailWithUID(path, uid)\
    char realpath[strlen(path) + strlen(ROOT) + 1 + strlen(uid) + 1 + 13 + 4];\
    strcpy(realpath, ROOT);\
    realpath[strlen(ROOT)] = 0;\
    strcat(realpath, path);\
    realpath[strlen(ROOT)+strlen(path)] = ';';\
    realpath[strlen(ROOT)+strlen(path)+1] = 0;\
    strcat(realpath, "uid=");\
    realpath[strlen(path) + strlen(ROOT) + 1 + 4] = 0;\
    strcat(realpath, uid);\
    realpath[strlen(path) + strlen(ROOT) + 1 + strlen(uid) + 4] = 0;\
    strcat(realpath, ";section=text");\
    realpath[strlen(path) + strlen(ROOT) + 1 + strlen(uid) + 13 + 4] = 0;

// get url with uid of mail without any specific section
#define getFullMailWithUID(path, uid)\
    char realpath[strlen(path) + strlen(ROOT) + 1 + strlen(uid) + 1 + 4];\
    strcpy(realpath, ROOT);\
    realpath[strlen(ROOT)] = 0;\
    strcat(realpath, path);\
    realpath[strlen(ROOT)+strlen(path)] = ';';\
    realpath[strlen(ROOT)+strlen(path)+1] = 0;\
    strcat(realpath, "uid=");\
    realpath[strlen(path) + strlen(ROOT) + 1 + 4] = 0;\
    strcat(realpath, uid);\
    realpath[strlen(path) + strlen(ROOT) + 1 + strlen(uid) + 4] = 0;

extern const int LOG;

#define EMAIL_ (((struct data*) fuse_get_context()->private_data) -> username)
#define PWD_ (((struct data*) fuse_get_context()->private_data) -> password)
#define ROOT (((struct data*) fuse_get_context()->private_data) -> root)
#define upload_text (((struct data*) fuse_get_context()->private_data) -> uploadText)
#define MOUNT "CS303"

void string_init(char **s)
{
    *s = (char *) malloc(1);
    (*s)[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, char **s)
{
    size_t new_len = strlen(*s) + size * nmemb;
    *s = realloc(*s, new_len + 1);
    if (*s == NULL)
    {
        fprintf(stderr, "realloc() failed\n");
        exit(1);
    }
    memcpy(*s + strlen(*s), ptr, size * nmemb);
    (*s)[new_len] = '\0';

    return size * nmemb;
}

char* gotoURL(char *path)
{
    char *result;
    string_init(&result);

    CURL *curl;
    CURLcode res = CURLE_OK;
    
    curl = curl_easy_init();
    if(curl) {
        setCred(curl);

        /* This is just the server URL */
        absoluteURL(path);
        curl_easy_setopt(curl, CURLOPT_URL, realpath);

        /* Save the output in result*/
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    
        /* Perform the custom request */
        res = curl_easy_perform(curl);
    
        /* Check for errors */
        if(res != CURLE_OK)
        if (LOG) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    
        /* Always cleanup */
        curl_easy_cleanup(curl);
    }
    
    return result;
}

int isDirectory(char *path)
{
    char *urlOutput = gotoURL(path); // getting the query output for fetching this url using imap

    int result = strcmp(urlOutput, ""); // check if its a directory

    free(urlOutput); // freeing the memory

    return result; // 0 if not a directory, non-0 for a directory
}

char* readMail(char *parent_dir, char *filename)
{
    // result to store query output
    char *result;
    string_init(&result);

    // get uid of the email or detect if no such file exists
    {
        CURL *curl;
        CURLcode res = CURLE_OK;
        
        curl = curl_easy_init();
        if(curl) {
            setCred(curl);

            /* This is just the server URL */
            absoluteURL(parent_dir);
            curl_easy_setopt(curl, CURLOPT_URL, realpath);

            /* Save the output in result*/
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

            /* Get mail uid from subject */
            char query[21+strlen(filename)+1];
            strcpy(query, "UID SEARCH SUBJECT \"\0");
            strcat(query, filename);
            strcat(query, "\"");
            query[21+strlen(filename)] = 0;
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, query);
        
            /* Perform the custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
            {
                if (LOG) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                printf("error: curl mishap, check internet or the url/path\n");
            }
        
            /* Always cleanup */
            curl_easy_cleanup(curl);
        }
    }

    if (!strcmp(result, "* SEARCH\r\n")) return result; // file doesn't exist

    getUID(result); // get uid of the email
    free(result); // free existing result
    string_init(&result); // reinitialize result

    // get contents of the email
    {
        CURL *curl;
        CURLcode res = CURLE_OK;
        
        curl = curl_easy_init();
        if(curl) {
            setCred(curl);

            /* This is just the server URL */
            getMailWithUID(parent_dir, uid);
            curl_easy_setopt(curl, CURLOPT_URL, realpath);

            /* Save the output in result*/
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        
            /* Perform the custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
            {
                if (LOG) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                printf("error: curl mishap, check internet\n");
            }
        
            /* Always cleanup */
            curl_easy_cleanup(curl);
        }
    }
    
    return result;
}

int isFile(char *path)
{
    size_t path_len = strlen(path);
    
    int filename_starts_at = -1; // to extract filename

    for (int i = path_len - 1; i > -1; i--) // reverse loop to detect last / as that will point to start of filename
    {
        if (path[i] == '/')
        {
            filename_starts_at = i + 1;
            break;
        }
    }

    if (filename_starts_at == -1) return 0; // not a file

    char filename[path_len - filename_starts_at + 1];
    strncpy(filename, path+filename_starts_at, path_len - filename_starts_at); // extracted filename stored
    filename[path_len-filename_starts_at] = 0;
    
    char parent_dir[filename_starts_at + 1];
    strncpy(parent_dir, path, filename_starts_at); // extracted parent directory path
    parent_dir[filename_starts_at] = 0;

    // get output -> mail content if it exists else the string "* SEARCH\r\n"
    char *output = readMail(parent_dir, filename);
    int result = strcmp(output, "* SEARCH\r\n");
    free(output); // freeing the output

    return result;
}

int getFileDetails(char *path, char **filename, char **content)
{
    size_t path_len = strlen(path);
    
    int filename_starts_at = -1; // to extract filename

    for (int i = path_len - 1; i > -1; i--) // reverse loop to detect last / as that will point to start of filename
    {
        if (path[i] == '/')
        {
            filename_starts_at = i + 1;
            break;
        }
    }

    if (filename_starts_at == -1) return 0; // not a file

    *filename = (char *) malloc(path_len - filename_starts_at + 1);
    strncpy(*filename, path+filename_starts_at, path_len - filename_starts_at); // extracted filename stored
    (*filename)[path_len-filename_starts_at] = 0;
    
    char parent_dir[filename_starts_at + 1];
    strncpy(parent_dir, path, filename_starts_at); // extracted parent directory path
    parent_dir[filename_starts_at] = 0;

    *content = readMail(parent_dir, *filename);

    return 1; // is a file path
}

int extractFileDetails(char *path, char **filename, char **parent_dir)
{
    size_t path_len = strlen(path);
    
    int filename_starts_at = -1; // to extract filename

    for (int i = path_len - 1; i > -1; i--) // reverse loop to detect last / as that will point to start of filename
    {
        if (path[i] == '/')
        {
            filename_starts_at = i + 1;
            break;
        }
    }

    if (filename_starts_at == -1) return 0; // not a file

    *filename = (char *) malloc(path_len - filename_starts_at + 1);
    strncpy(*filename, path+filename_starts_at, path_len - filename_starts_at); // extracted filename stored
    (*filename)[path_len-filename_starts_at] = 0;
    
    *parent_dir = (char *) malloc(filename_starts_at + 1);
    strncpy(*parent_dir, path, filename_starts_at); // extracted parent directory path
    (*parent_dir)[filename_starts_at] = 0;

    return 1; // is a file path
}

static size_t upload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
    size_t bytes_read = *((size_t *) userp);
    const char *data;
    size_t room = size * nmemb;

    if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) return 0;

    data = upload_text + bytes_read;

    if (*data)
    {
        size_t len = strlen(data);
        if(room < len)
        len = room;
        memcpy(ptr, data, len);
        *((size_t *) userp) += len;

        return len;
    }
    return 0;
}

int createFile(char *path, char *content)
{
    upload_text = content;
    
    CURL *curl;
    CURLcode res = CURLE_OK;

    curl = curl_easy_init();
    if(curl) {
        long infilesize;
        size_t bytes_read = 0;

        setCred(curl);

        /* Set path where the file needs to be created */
        curl_easy_setopt(curl, CURLOPT_URL, path);

        /* Using a callback function to specify the data */
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, upload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &bytes_read);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        infilesize = strlen(upload_text);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, infilesize);

        /* Perform the append */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        /* Always cleanup */
        curl_easy_cleanup(curl);
    }

    upload_text = 0;

    return (int)res;
}

int createDirectory(char *path)
{
    CURL *curl;
    CURLcode res = CURLE_OK;
    
    curl = curl_easy_init();
    if(curl) {
        setCred(curl);
    
        /* This is just the server URL */
        curl_easy_setopt(curl, CURLOPT_URL, ROOT);
    
        /* Set the CREATE command specifying the new folder name */
        char mod_path[7 + strlen(MOUNT) + strlen(path) + 1];
        strcpy(mod_path, "CREATE ");
        mod_path[7] = 0;
        strcat(mod_path, MOUNT);
        mod_path[7 + strlen(MOUNT)] = 0;
        strcat(mod_path, path);
        mod_path[7 + strlen(MOUNT) + strlen(path)] = 0;
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, mod_path); // create a directory
    
        /* Perform the custom request */
        res = curl_easy_perform(curl);
    
        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    
        /* Always cleanup */
        curl_easy_cleanup(curl);
    }

    return res;
}

int deleteFile(char *path)
{
    char *result;
    string_init(&result);

    // get file details
    char *filename, *parent_dir;
    extractFileDetails(path, &filename, &parent_dir);

    absoluteURL(parent_dir); // full url stored in realpath

    // get uid of the email or detect if no such file exists
    {
        CURL *curl;
        CURLcode res = CURLE_OK;
        
        curl = curl_easy_init();
        if(curl) {
            setCred(curl);

            /* This is just the server URL */
            absoluteURL(parent_dir);
            curl_easy_setopt(curl, CURLOPT_URL, realpath);

            /* Save the output in result*/
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

            /* Get mail uid from subject */
            char query[21+strlen(filename)+1];
            strcpy(query, "UID SEARCH SUBJECT \"\0");
            strcat(query, filename);
            strcat(query, "\"");
            query[21+strlen(filename)] = 0;
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, query);
        
            /* Perform the custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
            {
                if (LOG) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                printf("error: curl mishap, check internet or the url/path\n");
            }
        
            /* Always cleanup */
            curl_easy_cleanup(curl);
        }
    }

    if (!strcmp(result, "* SEARCH\r\n")) return ENOENT; // file doesn't exist

    getUID(result); // get uid of the email
    free(result); // free existing result

    CURLcode res;

    {
        CURL *curl;
        res = CURLE_OK;
        
        curl = curl_easy_init();
        if(curl) {
            /* Set username and password */
            setCred(curl);
        
            /* This is the mailbox folder to select */
            curl_easy_setopt(curl, CURLOPT_URL, realpath);
        
            /* Set the STORE command with the Deleted flag for message of given uid */

            // store request in the string "request"
            char request[27 + strlen(uid)];
            strcpy(request, "UID STORE \0");
            strcat(request, uid);
            request[11 + strlen(uid)] = 0;
            strcat(request, " +Flags \\Deleted");
            request[27+strlen(uid)] = 0;

            // setup requet
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request);
        
            /* Perform the custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            else {
            /* Set the EXPUNGE command, although you can use the CLOSE command if you
            * do not want to know the result of the STORE */
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "EXPUNGE");
        
            /* Perform the second custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
        
            /* Always cleanup */
            curl_easy_cleanup(curl);
        }
    }

    // freeing
    free(filename);
    free(parent_dir);

    if (res != CURLE_OK) return ECONNABORTED;
    return 0;
}

char* getFiles(char *path)
{
    char *s;
    string_init(&s);
    CURL *curl;
    CURLcode res = CURLE_OK;

    curl = curl_easy_init();
    if(curl) {
        /* Set the username and password */
        setCred(curl);

        /* Get absolute url in realpath */
        absoluteURL(path);
        curl_easy_setopt(curl, CURLOPT_URL, realpath);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "UID SEARCH ALL"); // get uids of all the files in directory

        /* Perform the fetch */
        res = curl_easy_perform(curl);

        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        /* Always cleanup */
        curl_easy_cleanup(curl);
    }

    return s;
}

int isEmpty(char *path)
{
    /**
     * check if the directory is empty or not before rmdir 
     */

    char *result = gotoURL(path); // obtain directory output
    int size = strlen(result);

    int ans = -1; // isEmtpy or not?

    int cnt = 0;

    for (int i = 0; i < size; i++) // count number of lines to know the number of folders in it
    {
        if (result[i] == '\n') cnt++;
    }

    if (cnt > 1) ans = 0; // if there is some folder, then return 0

    if (ans == 0) return ans;

    free(result);

    result = getFiles(path);
    ans = strcmp(result, "* SEARCH\r\n");
    // 0 if there is some file else non-0

    free(result);

    if (ans == 0) return 1;
    return 0;
}

int deleteDir(char *path)
{
    if (strcmp(path, "/") == 0)
    {
        printf("error: can't remove root directory\n");;
        return EACCES;
    }
    if (!isEmpty)
    {
        printf("error: directory not empty\n");
        return ENOTEMPTY;
    }
    CURL *curl;
    CURLcode res = CURLE_OK;
    
    curl = curl_easy_init();
    if(curl) {
        /* Set username and password */
        setCred(curl);
    
        /* This is just the server URL */
        curl_easy_setopt(curl, CURLOPT_URL, ROOT);
    
        // construct the delete request as a string
        char request[7 + strlen(MOUNT) + strlen(path) + 1];
        strcpy(request, "DELETE \0");
        strcat(request, MOUNT);
        request[7 + strlen(MOUNT)] = 0;
        strcat(request, path);
        request[7 + strlen(MOUNT) + strlen(path)] = 0;

        /* Set the DELETE command specifying the folder name */
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request); // delete directory
    
        /* Perform the custom request */
        res = curl_easy_perform(curl);
    
        /* Check for errors */
        if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    
        /* Always cleanup */
        curl_easy_cleanup(curl);
    }

    if (res != CURLE_OK) return ECONNABORTED;
    return 0;
}

char* nameFromUID(char *parent_dir, char *uid)
{
    char *result;
    string_init(&result);

    // get contents of the email
    {
        CURL *curl;
        CURLcode res = CURLE_OK;
        
        curl = curl_easy_init();
        if(curl) {
            setCred(curl);

            /* This is just the server URL */
            getFullMailWithUID(parent_dir, uid);
            curl_easy_setopt(curl, CURLOPT_URL, realpath);

            /* Save the output in result*/
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
        
            /* Perform the custom request */
            res = curl_easy_perform(curl);
        
            /* Check for errors */
            if(res != CURLE_OK)
            {
                if (LOG) printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                printf("error: curl mishap, check internet\n");
            }
        
            /* Always cleanup */
            curl_easy_cleanup(curl);
        }
    }

    int start = 9;
    int size = 0;
    
    while (1) if (result[start + size] != '\r') size++; else break;

    char *filename = (char *) malloc(size+1);
    strncpy(filename, result+9, size);
    filename[size] = 0;

    return filename;
}

char** getAllFiles(char *path)
{
    char *output = getFiles(path);
    size_t len = strlen(output);

    char uids[strlen(output) - 10 + 1];
    strncpy(uids, output + 8, strlen(output) - 10);
    uids[strlen(output) - 10] = 0;

    int num_files = 0, size_uids = strlen(uids);
    for (int i = 0; i < size_uids; i++)
    {
        if (uids[i] == ' ') num_files++;
    }

    char **files = (char**) malloc(sizeof(char*) * (num_files + 1));
    files[num_files] = 0;

    FILE *stream = fmemopen(uids, strlen(uids), "r");
    char uid[10];
    for (int i = 0; i < num_files; i++)
    {
        fscanf(stream, "%s", uid);
        files[i] = nameFromUID(path, uid);
    }

    return files;
}

