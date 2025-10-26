/*
   Copyright 2021 Paranoid-Dev

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <Python.h> // >= Python3.6
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

const char *title, *author, *summary, *info, *filename, *totalchapters, *url;
char updated[32];
char published[32];
char datetime[22];
char mark[32];
time_t updatedunixtimestamp;
time_t publishedunixtimestamp;
PyObject *mainModule;
int p = 1;   // for argument parsing
int f = 1;   // output file format
int fname = 1; // what to use for output file name
int down = 0;
int argvurl;
size_t l;    // buffer size
int j = 1;   // total chapters
int errnum = 0;
int t = 2;   // threads

void help () {
    puts(
        " ________________________________________________________________________________________ \n"
        "                 Paranoid FanFiction Downloader v1.3.1.2  by Paranoid-Dev                 \n"
        "                       https://github.com/Paranoid-Dev/ParanoidFFD                        \n"
        " ________________________________________________________________________________________ \n"
        "                                                                                          \n"
        " Usage    : ParanoidFFD <options>                                                         \n"
        "                                                                                          \n"
        " Options  :                                                                               \n"
        "   -u <fanfiction url> : download from <fanfiction url>                                   \n"
        "   -f <FORMAT>         : save fanfiction as <FORMAT>                                      \n"
        "                         currently supported format : txt, epub                           \n"
        "                         default : txt                                                    \n"
        "   -o <FILE_NAME>      : save fanfiction as <FILE_NAME>.<FORMAT>                          \n"
        "                         default : fanfiction_title.<FORMAT>                              \n"
        "   -t <n>              : number of parallel download threads, use a positive integer      \n"
        "                         default : 2                                                      \n"
        "                         WARNING; high threadcount use for long fics may trigger captchas \n"
        "   --version           : show ParanoidFFD version                                         \n"
        "   --check-update      : check for new updates                                            \n"
        "   -h , --help         : show this page                                                   \n"
        "                                                                                          \n"
        " Examples :                                                                               \n"
        "   ParanoidFFD -u \"my/fan/fiction/url\"                                                    \n"
        "   ParanoidFFD -f epub -t 4 -o \"my fanfiction save\" -u \"my/fan/fiction/url\"               \n"
        "   ParanoidFFD --check-update                                                             \n"
        " ________________________________________________________________________________________ \n"
    );
}

const char * HTML_SpecialChar_encode (const char* HTML) {
    char HTMLbuf[strlen(HTML)+20];
    sprintf(HTMLbuf, "out = \"\"\"%s\"\"\"",HTML);
    PyRun_SimpleString(HTMLbuf);
    PyRun_SimpleString(
        "out = re.sub('&','&amp;',out, flags=re.DOTALL) \n"
        "out = re.sub('<','&lt;',out, flags=re.DOTALL) \n"
        "out = re.sub('>','&gt;',out, flags=re.DOTALL) \n"
        "out = re.sub('\"','&quot;',out, flags=re.DOTALL) \n"
        "out = re.sub(\"'\",'&#x27;',out, flags=re.DOTALL) \n"
    );
    PyObject *HTMLencodedPy = PyObject_GetAttrString(mainModule, "out");
    return PyUnicode_AsUTF8(HTMLencodedPy);
}

int initializePy () {
    Py_Initialize();
    mainModule = PyImport_AddModule("__main__");
    return PyRun_SimpleString(
        // ====================== Python Prelude (Selenium Manager + threads + Homebrew helper) ======================
        "import os, platform \n"
        "from selenium import webdriver \n"
        "from selenium.webdriver.common.by import By \n"
        "from selenium.webdriver.support.ui import WebDriverWait \n"
        "from selenium.webdriver.support import expected_conditions as EC \n"
        "from joblib import Parallel, delayed \n"
        "import re, random, zipfile, base64, psutil \n"
        "from time import sleep \n"
        "\n"
        "# Prevent joblib from re-spawning this C binary as Python (causing -c/-m flags)\n"
        "os.environ.pop('PYTHONEXECUTABLE', None)\n"
        "\n"
        "# --- Optional Homebrew helper (user-provided snippet) ---\n"
        "def make_chrome_homebrew(headless=True):\n"
        "    options = webdriver.ChromeOptions()\n"
        "    if headless:\n"
        "        options.add_argument('--headless=new')\n"
        "    # Homebrew puts chromedriver on PATH; Selenium/Chrome will be found\n"
        "    return webdriver.Chrome(options=options)\n"
        "\n"
        "def _kill_by_flag(flag): \n"
        "    if not flag: return \n"
        "    for process in psutil.process_iter(): \n"
        "        try: \n"
        "            if flag in process.cmdline(): \n"
        "                process.terminate() \n"
        "        except Exception: \n"
        "            pass \n"
        "\n"
        "def make_chrome(headless=True): \n"
        "    # If user explicitly asks to use Homebrew chromedriver on macOS, try that first.\n"
        "    if platform.system() == 'Darwin' and os.getenv('PARANOIDFFD_USE_HOMEBREW') == '1':\n"
        "        drv = make_chrome_homebrew(headless=headless)\n"
        "        flag = None  # no per-driver flag when using Homebrew helper\n"
        "        try:\n"
        "            og = drv.execute_script('return navigator.userAgent')\n"
        "            try:\n"
        "                drv.execute_cdp_cmd('Network.setUserAgentOverride', {'userAgent': og.replace('Headless','')})\n"
        "            except Exception:\n"
        "                pass\n"
        "        except Exception:\n"
        "            pass\n"
        "        return drv, flag\n"
        "\n"
        "    # Default path: Selenium Manager (no external driver binary committed)\n"
        "    flag = f\"--ParanoidFFD_CHROMEID={random.randint(100000, 999999)}\" \n"
        "    options = webdriver.ChromeOptions() \n"
        "    options.add_argument('start-maximized') \n"
        "    options.add_experimental_option('excludeSwitches', ['enable-automation']) \n"
        "    options.add_argument('--disable-blink-features=AutomationControlled') \n"
        "    options.add_argument(flag) \n"
        "    if headless:\n"
        "        options.add_argument('--headless=new') \n"
        "    drv = webdriver.Chrome(options=options)  # Selenium Manager resolves driver \n"
        "    try:\n"
        "        og = drv.execute_script('return navigator.userAgent') \n"
        "        try:\n"
        "            drv.execute_cdp_cmd('Network.setUserAgentOverride', {'userAgent': og.replace('Headless','')}) \n"
        "        except Exception:\n"
        "            pass \n"
        "    except Exception:\n"
        "        pass \n"
        "    return drv, flag \n"
        "\n"
        "def fetch_chapter(url): \n"
        "    for _ in range(1,10): \n"
        "        driver = None; flag = None \n"
        "        try: \n"
        "            print('Downloading [', url, ']...') \n"
        "            driver, flag = make_chrome(True) \n"
        "            driver.get(url) \n"
        "            el = WebDriverWait(driver, 20).until(EC.presence_of_element_located((By.ID, 'storytext'))) \n"
        "            chapter = el.get_attribute('innerHTML') \n"
        "            chapter = re.sub(r'<div.*?</div>', r'', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<p[^>]*?(center|left|right|justify).*?>', r'<p style=\"text-align: \\1; text-align-last: \\1;\">', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<p (?![^>]*\\b(?:center|right|left|justify)\\b).*?>', r'<p>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<i .*?>', r'<i>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<em .*?>', r'<em>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<strong .*?>', r'<strong>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<b .*?>', r'<b>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'<u .*?>', r'<u>', chapter, flags=re.DOTALL) \n"
        "            chapter = re.sub(r'(<hr|<br).*?>', r'\\1 />', chapter, flags=re.DOTALL) \n"
        "            print('Finished downloading [', url, ']') \n"
        "            try: driver.quit() \n"
        "            finally: _kill_by_flag(flag) \n"
        "            return chapter \n"
        "        except Exception as e: \n"
        "            print('Failed to download', url, '—', str(e), 'Retrying in 15s...') \n"
        "            try: \n"
        "                if driver: driver.quit() \n"
        "            finally: _kill_by_flag(flag) \n"
        "            sleep(15) \n"
        "    print('Failed to download', url, 'Try again later (with less parallel threads) or open an issue at github.com/Paranoid-Dev/ParanoidFFD') \n"
        "    return f'Failed to download {url}' \n"
        "\n"
        "def download_all(urls, n_jobs): \n"
        "    # Force threading backend to avoid loky subprocesses (-c/-m) conflicting with the C CLI\n"
        "    try:\n"
        "        return Parallel(\n"
        "            n_jobs=n_jobs,\n"
        "            prefer='threads',\n"
        "            backend='threading'\n"
        "        )(delayed(fetch_chapter)(u) for u in urls) \n"
        "    except Exception as e:\n"
        "        print('Parallel(threading) failed, falling back to serial:', e)\n"
        "        return [fetch_chapter(u) for u in urls] \n"
        // ====================== End Prelude ======================
    );
}

void Py_Launcher () {
    while (initializePy ()) {
        ++errnum;
        if (errnum == 10) {
            puts("\nParanoidFFD Python failed to initilize - please open an issue at github.com/Paranoid-Dev/ParanoidFFD");
            Py_Finalize();
            exit(2);
        }
        puts("\n\nParanoidFFD Python intialization error - Trying again..\n\n");
        Py_Finalize();
    }
    errnum = 0;
    puts("ParanoidFFD Py initialized");
}

void Webdriver_Launcher () {
    // Long-lived driver 'chrome' for metadata; keep its per-driver flag in META_FLAG.
    while (PyRun_SimpleString(
        "chrome, META_FLAG = make_chrome(True) \n"
    )) {
        ++errnum;
        if (errnum == 10) {
            puts("\nParanoidFFD Webdriver failed to initilize - please open an issue at github.com/Paranoid-Dev/ParanoidFFD");
            PyRun_SimpleString(
                "try: chrome.quit()\n"
                "except Exception: pass\n"
                "_kill_by_flag(META_FLAG)\n"
            );
            Py_Finalize();
            exit(3);
        }
        puts("\n\nParanoidFFD Webdriver intialization error - Trying again..\n\n");
        PyRun_SimpleString(
            "try: chrome.quit()\n"
            "except Exception: pass\n"
            "_kill_by_flag(META_FLAG)\n"
        );
    }
    errnum = 0;
    puts("ParanoidFFD Webdriver initialized");
}

void fic_info () {
    Webdriver_Launcher ();

    time_t now; // date-time
    time(&now);
    struct tm* now_tm;
    now_tm = gmtime(&now);
    strftime (datetime, 22, "%FT%TZ", now_tm);
    strftime (mark, 32, "on %F, at %T", now_tm);

    puts("Parsed fanfiction url\nFetching fanfiction info ...");

    PyRun_SimpleString("chrome.get(url)");
    puts("Downloading info ...");
    while (PyRun_SimpleString(
        "root = WebDriverWait(chrome, 20).until(EC.presence_of_element_located((By.ID, 'profile_top'))) \n"
        "title = root.find_element(By.TAG_NAME, 'b').text \n"
        "author = root.find_element(By.XPATH, './a[1]').text \n"
        "summary = root.find_element(By.XPATH, './div').text \n"
        "try:\n"
        "    info = root.find_element(By.XPATH, './span[4]').text \n"
        "except Exception:\n"
        "    info = root.find_element(By.XPATH, './span[3]').text \n"
        "try:\n"
        "    chapterlist = chrome.find_element(By.ID, 'chap_select').text \n"
        "except Exception:\n"
        "    chapterlist = title \n"
        "updated = root.find_element(By.XPATH, './span/span[1]').get_attribute('outerHTML') \n"
        "try:\n"
        "    published = root.find_element(By.XPATH, './span/span[2]').get_attribute('outerHTML') \n"
        "except Exception:\n"
        "    published = root.find_element(By.XPATH, './span/span[1]').get_attribute('outerHTML') \n"
    )) {
        ++errnum;
        if (errnum == 10) {
            puts("\nParanoidFFD failed to download fic info - please open an issue at github.com/Paranoid-Dev/ParanoidFFD");
            PyRun_SimpleString(
                "try: chrome.quit()\n"
                "except Exception: pass\n"
                "_kill_by_flag(META_FLAG)\n"
            );
            Py_Finalize();
            exit(4);
        }
        puts("\n\nFailed to download fic info - Trying again..\n\n");
        PyRun_SimpleString(
            "try: chrome.quit()\n"
            "except Exception: pass\n"
            "_kill_by_flag(META_FLAG)\n"
        );
        Webdriver_Launcher ();
        PyRun_SimpleString("chrome.get(url)");
    }
    errnum = 0;
    PyRun_SimpleString(
        "try: chrome.quit()\n"
        "except Exception: pass\n"
        "_kill_by_flag(META_FLAG)\n"
    );

    PyObject *titlePy = PyObject_GetAttrString(mainModule, "title");
    title = PyUnicode_AsUTF8(titlePy);                                              // title
    PyObject *authorPy = PyObject_GetAttrString(mainModule, "author");
    author = PyUnicode_AsUTF8(authorPy);                                            // author
    PyObject *summaryPy = PyObject_GetAttrString(mainModule, "summary");
    summary = PyUnicode_AsUTF8(summaryPy);                                          // summary

    PyRun_SimpleString(
        "updated = re.sub(r'.*?\"(.*?)\".*', r'\\1', updated, flags=re.DOTALL) \n"
        "published = re.sub(r'.*?\"(.*?)\".*', r'\\1', published, flags=re.DOTALL) \n"
    );

    PyObject *updatedPy = PyObject_GetAttrString(mainModule, "updated");
    updatedunixtimestamp = strtol(PyUnicode_AsUTF8(updatedPy), NULL, 10);
    struct tm *updatedtimestamp = gmtime(&updatedunixtimestamp);
    strftime (updated, 32, "on %F, at %T", updatedtimestamp);
    PyObject *publishedPy = PyObject_GetAttrString(mainModule, "published");
    publishedunixtimestamp = strtol(PyUnicode_AsUTF8(publishedPy), NULL, 10);
    struct tm *publishedtimestamp = gmtime(&publishedunixtimestamp);
    strftime (published, 32, "on %F, at %T", publishedtimestamp);

    PyRun_SimpleString(
        "info = re.sub(r'(Reviews|Favs|Follows|Published|Updated).*?- ', r'', info, flags=re.DOTALL) \n"
        "info = re.sub(r'- ', r'\\n', info, flags=re.DOTALL) \n"
        "info = re.sub(r'Rated: ', r'', info, flags=re.DOTALL) \n"
        "info = re.sub(r'id:.*$', r'', info, flags=re.DOTALL) \n"
    );
    PyObject *infoPy = PyObject_GetAttrString(mainModule, "info");
    info = PyUnicode_AsUTF8(infoPy);                                                // info
    PyObject *chapterlistPy = PyObject_GetAttrString(mainModule, "chapterlist");    // chapter-list
    totalchapters = PyUnicode_AsUTF8(chapterlistPy); // for epub output

    puts("Fanfiction info downloaded\nParsing chapters ...");

    const char * chapterlist = PyUnicode_AsUTF8(chapterlistPy);
    while(chapterlist) {
        const char * nextLine = strchr(chapterlist, '\n');
        int chapterlistLen = nextLine ? (nextLine-chapterlist) : (int)strlen(chapterlist);
        ++j;
        chapterlist = nextLine ? (nextLine+1) : NULL;
    }
    --j;
}

int main (int argc, char *argv[]) {
    if (argc == 1) {
        puts(
            " ________________________________________________________________________________________ \n"
            "                 Paranoid FanFiction Downloader v1.3.1.2  by Paranoid-Dev                 \n"
            "                       https://github.com/Paranoid-Dev/ParanoidFFD                        \n"
            " ________________________________________________________________________________________ \n"
            " \"ParanoidFFD --help\" to show help page                                                   \n"
        );
    }
    else {
        while (p < argc) {
            if (strcmp(argv[p], "--version") == 0) {
                puts("ParanoidFFD 1.3.1.2");
            }
            else if (strcmp(argv[p], "--help") == 0) {
                help ();
            }
            else if (strcmp(argv[p], "-h") == 0) {
                help ();
            }
            else if (strcmp(argv[p], "-t") == 0) {
                ++p;
                t = atoi(argv[p]);
                if (t < 1) {
                    printf("invalid thread count : %s\n", argv[p]);
                    exit(5);
                }
            }
            else if (strcmp(argv[p], "-f") == 0) {
                ++p;
                if (strcmp(argv[p], "txt") == 0) {
                    f = 1;  // txt
                }
                else if (strcmp(argv[p], "epub") == 0) {
                    f = 2;  // epub
                }
                else {
                    printf("invalid format : %s\n", argv[p]);
                    exit(5);
                }
            }
            else if (strcmp(argv[p], "-o") == 0) {
                ++p;
                fname = 0;  // not using default filename
                filename = argv[p];
            }
            else if (strcmp(argv[p], "-u") == 0) {
                ++p;
                argvurl = p;
                down = 1;
                l = strlen(argv[p]) + 12;
            }
            else if (strcmp(argv[p], "--check-update") == 0) {
                Py_Launcher ();
                Webdriver_Launcher ();
                PyRun_SimpleString(
                    "chrome.get('https://raw.githubusercontent.com/Paranoid-Dev/ParanoidFFD/main/updates%20history/1.3.1.2-n') \n"
                    "nextver = chrome.find_element(By.TAG_NAME, 'pre').text \n"
                );
                PyObject *nextverPy = PyObject_GetAttrString(mainModule, "nextver");
                const char * nextver = PyUnicode_AsUTF8(nextverPy);
                if (strcmp(nextver, "NA") == 0) {
                    puts("ParanoidFFD is up to date! ParanoidFFD v1.3.1.2 by Paranoid-Dev");
                }
                else {
                    puts("ParanoidFFD isn't up to date. Fetching updates info..\nNew version : \n");
                    PyRun_SimpleString(
                        "chrome.get(nextver) \n"
                        "print(chrome.find_element(By.TAG_NAME, 'pre').text) \n"
                    );
                    puts("\nCurrent version : ParanoidFFD v1.3.1.2");
                }
                PyRun_SimpleString(
                    "try: chrome.quit()\n"
                    "except Exception: pass\n"
                    "_kill_by_flag(META_FLAG)\n"
                );
                Py_Finalize();
            }
            else {
                printf("invalid argument : %s \n", argv[p]);
                exit(5);
            }
            ++p;
        }

        if (down) {
            printf("Downloading %s with %d threads...\n",argv[argvurl],t);

            Py_Launcher ();
            char buf[l];
            sprintf(buf, "yurl = \"%s\"",argv[argvurl]);
            PyRun_SimpleString(buf);
            PyRun_SimpleString(
                "zurl = yurl.replace('https://', '') \n"
                "furl = zurl.replace('http://', '') \n"
                "durl = furl.replace('www.', '') \n"
                "aurl = durl.replace('m.', '') \n"
                "burl = '/'.join(aurl.split('/', 3)[:3]) \n"
                "url = f\"https://www.{burl}/\" \n"
            );
            PyObject *urlPy = PyObject_GetAttrString(mainModule, "url");
            url = PyUnicode_AsUTF8(urlPy); // url

            fic_info ();
            int z = 1;
            PyObject *chapterlistPy = PyObject_GetAttrString(mainModule, "chapterlist"); // chapter-list
            const char * chapterlist = PyUnicode_AsUTF8(chapterlistPy);
            int chapterlistsize = (int) PyObject_Size(chapterlistPy);
            char chaptername[j+2][chapterlistsize*6]; // *6 for html encoding
            while(chapterlist) {
                const char * nextLine = strchr(chapterlist, '\n');
                int chapterlistLen = nextLine ? (nextLine-chapterlist) : (int)strlen(chapterlist);
                if (chaptername[z]) {
                    memcpy(chaptername[z], chapterlist, chapterlistLen);
                    chaptername[z][chapterlistLen] = '\0';
                    printf("[ %s ]\n", chaptername[z]);
                    ++z;
                }
                else puts("chapter parsing failed - please open an issue at github.com/Paranoid-Dev/ParanoidFFD");
                chapterlist = nextLine ? (nextLine+1) : NULL;
            }
            puts("Parsed chapters");

            if (fname) filename = title; // default filename is title

            char threadPy[32];
            sprintf(threadPy, "t = %d",t);
            PyRun_SimpleString(threadPy);

            int digits = snprintf(0,0,"%+d",j)-1;
            char chaplist[strlen(url)*j + j*digits + 12];
            char chaplistbuf[strlen(url)*j + j*digits + 12];

            sprintf(chaplistbuf, "urls = ['%s'", url);
            for (int i = 2; i <= j; ++i) { // put chapter urls in list
                sprintf(chaplist, "%s", chaplistbuf);
                sprintf(chaplistbuf, "%s,'%s%d'", chaplist, url, i);
            }
            sprintf(chaplist, "%s]", chaplistbuf);
            PyRun_SimpleString(chaplist);

            // THREAD-BASED parallel download (with serial fallback)
            PyRun_SimpleString("chapters = download_all(urls, t)");

            puts("Download finished\nWriting to file ...");

            if (f == 1) { // save to text
                int m = 2*(int)strlen(totalchapters) + (int)strlen(title) + (int)strlen(author) + (int)strlen(info) + (int)strlen(summary) + (int)strlen(mark) + 215;

                for (int b = 1; b <= j; ++b) {
                    char buf3[17+digits];
                    sprintf(buf3, "out = chapters[%d]",b-1);
                    PyRun_SimpleString(buf3);
                    PyObject *chapterPy = PyObject_GetAttrString(mainModule, "out");
                    int chaptersize = (int) PyObject_Size(chapterPy);
                    m = m + chaptersize + 12;
                }

                static char buf1[10000000];
                static char buf2[10000000];
                sprintf(buf2, "%s\nBy %s\n\n%sPublished %s\nUpdated %s\nDownloaded with ParanoidFFD, %s\n\n%s\n\n\nChapters\n\n",title,author,info,published,updated,mark,summary);

                for (int b = 1; b <= j; ++b) {
                    sprintf(buf1, "%s%s\n",buf2,chaptername[b]);
                    sprintf(buf2, "%s",buf1);
                }
                sprintf(buf1, "%s\n\n\n",buf2);

                char buf3[17+digits];
                for (int b = 1; b <= j; ++b) {
                    sprintf(buf3, "out = chapters[%d]",b-1);
                    PyRun_SimpleString(buf3);
                    PyRun_SimpleString(
                        "out = re.sub('[*]','\\\\\\\\*',out, flags=re.DOTALL) \n"
                        "out = re.sub('\\\\\\\\','\\\\\\\\\\\\\\\\',out, flags=re.DOTALL) \n"
                        "out = re.sub('__','\\\\__',out, flags=re.DOTALL) \n"
                        "out = re.sub('</p>','\\n\\n',out, flags=re.DOTALL) \n"
                        "out = re.sub('<p.*?>','',out, flags=re.DOTALL) \n"
                        "out = re.sub('<br.*?>','\\\\\\\\\\n',out, flags=re.DOTALL) \n"
                        "out = re.sub('<hr.*?>','- — - — - — -\\n\\n',out, flags=re.DOTALL) \n"
                        "out = re.sub('<i.*?>','*',out, flags=re.DOTALL) \n"
                        "out = re.sub('</i>','*',out, flags=re.DOTALL) \n"
                        "out = re.sub('<em.*?>','*',out, flags=re.DOTALL) \n"
                        "out = re.sub('</em>','*',out, flags=re.DOTALL) \n"
                        "out = re.sub('<strong.*?>','**',out, flags=re.DOTALL) \n"
                        "out = re.sub('</strong>','**',out, flags=re.DOTALL) \n"
                        "out = re.sub('<b.*?>','**',out, flags=re.DOTALL) \n"
                        "out = re.sub('</b>','**',out, flags=re.DOTALL) \n"
                        "out = re.sub('<u.*?>','__',out, flags=re.DOTALL) \n"
                        "out = re.sub('</u>','__',out, flags=re.DOTALL) \n"
                        "out = re.sub('&lt;','<',out, flags=re.DOTALL) \n"
                        "out = re.sub('&gt;','>',out, flags=re.DOTALL) \n"
                        "out = re.sub('&quot;','\"',out, flags=re.DOTALL) \n"
                        "out = re.sub('&#x27;',\"'\",out, flags=re.DOTALL) \n"
                        "out = re.sub('&amp;','&',out, flags=re.DOTALL) \n"
                    );
                    PyObject *chapterPy = PyObject_GetAttrString(mainModule, "out");
                    const char *chapter = PyUnicode_AsUTF8(chapterPy);
                    sprintf(buf2, "%s| %s |\n\n",buf1,chaptername[b]);
                    sprintf(buf1, "%s%s\n\n",buf2,chapter);
                }

                printf("Saving to \"%s.txt\"\n",filename);
                FILE *fp;
                char output[strlen(filename)+5];
                sprintf(output, "%s.txt",filename);
                fp = fopen(output, "w+");
                fprintf(fp, "%s",buf1);
                fclose(fp);
                printf("Finished!\n");

            }
            else if (f == 2) { // save to epub
                title = HTML_SpecialChar_encode (title);
                author = HTML_SpecialChar_encode (author);
                summary = HTML_SpecialChar_encode (summary);
                info = HTML_SpecialChar_encode (info);
                totalchapters = HTML_SpecialChar_encode (totalchapters);
                url = HTML_SpecialChar_encode (url);
                for (int b = 1; b <= j; ++b) {
                    const char *chapternamebuf = chaptername[b];
                    chapternamebuf = HTML_SpecialChar_encode (chapternamebuf);
                    sprintf(chaptername[b], "%s",chapternamebuf);
                }

                unsigned char pre_ParanoidFFD_Template_epub[1275] = {0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbd, 0x0c, 0x54, 0x52, 0x6f, 0x61, 0xab, 0x2c, 0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x6d, 0x69, 0x6d, 0x65, 0x74, 0x79, 0x70, 0x65, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x65, 0x70, 0x75, 0x62, 0x2b, 0x7a, 0x69, 0x70, 0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x5d, 0x5b, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x1c, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x55, 0x54, 0x09, 0x00, 0x03, 0xb9, 0xb2, 0x39, 0x60, 0xba, 0xb2, 0x39, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0x29, 0x62, 0x5b, 0x52, 0xef, 0x71, 0x90, 0xc7, 0xa5, 0x00, 0x00, 0x00, 0x6f, 0x01, 0x00, 0x00, 0x2d, 0x00, 0x1c, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x69, 0x62, 0x6f, 0x6f, 0x6b, 0x73, 0x2e, 0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x2d, 0x6f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x55, 0x54, 0x09, 0x00, 0x03, 0xbe, 0xb9, 0x39, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x85, 0x90, 0x31, 0x0e, 0xc2, 0x30, 0x0c, 0x45, 0xf7, 0x9e, 0x22, 0xca, 0x88, 0x14, 0x0a, 0x1b, 0x43, 0x5a, 0x36, 0x4e, 0x00, 0x33, 0x8a, 0x12, 0x17, 0x59, 0xb4, 0x76, 0x94, 0x98, 0x0a, 0x6e, 0x4f, 0x4b, 0x61, 0xa2, 0x05, 0x8f, 0xdf, 0xef, 0xdb, 0xfe, 0xb6, 0xfb, 0x7b, 0xd7, 0xaa, 0x1e, 0x52, 0x46, 0xa6, 0x4a, 0x6f, 0xd7, 0x1b, 0xad, 0x80, 0x3c, 0x07, 0xa4, 0x4b, 0xa5, 0x4f, 0xc7, 0x83, 0xd9, 0xe9, 0x7d, 0x5d, 0xd8, 0x80, 0x39, 0xb6, 0xee, 0x71, 0xe6, 0x28, 0x03, 0x98, 0xeb, 0x42, 0x0d, 0x65, 0x07, 0x49, 0x1a, 0x4e, 0x9d, 0x22, 0xd7, 0x41, 0xa5, 0x57, 0x7a, 0xd2, 0x5f, 0xbd, 0x89, 0x7c, 0x77, 0x72, 0x04, 0x8f, 0x0d, 0x42, 0x30, 0x0d, 0x93, 0x64, 0x5d, 0x4b, 0xba, 0x81, 0x2d, 0x27, 0x66, 0xc9, 0xc4, 0x09, 0x81, 0xc4, 0x8d, 0x8a, 0x69, 0xd9, 0x5f, 0x75, 0x4d, 0x4c, 0x7f, 0x5d, 0x48, 0x02, 0xc9, 0x79, 0xc1, 0x1e, 0xe6, 0xd6, 0xd8, 0xf2, 0x73, 0xf4, 0x7c, 0x06, 0x8c, 0x2e, 0x2c, 0xc6, 0xe0, 0x08, 0x64, 0x84, 0x4d, 0x8e, 0x09, 0x46, 0xec, 0xf7, 0x78, 0x5b, 0x7e, 0x7d, 0xed, 0x09, 0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0xce, 0x95, 0x54, 0x52, 0xaa, 0x1e, 0x0e, 0x69, 0x95, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x00, 0x00, 0x16, 0x00, 0x1c, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x63, 0x6f, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x65, 0x72, 0x2e, 0x78, 0x6d, 0x6c, 0x55, 0x54, 0x09, 0x00, 0x03, 0x74, 0xda, 0x30, 0x60, 0x22, 0xb9, 0x38, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x55, 0x8e, 0x4d, 0x0e, 0xc2, 0x20, 0x10, 0x85, 0xf7, 0x3d, 0x05, 0x61, 0x6b, 0x5a, 0x74, 0x4b, 0xa0, 0x3d, 0xcb, 0x48, 0xa7, 0x4a, 0x84, 0x19, 0x02, 0xd4, 0xe8, 0xed, 0xa5, 0x5d, 0x34, 0x75, 0xf9, 0xf2, 0x7e, 0xbe, 0x67, 0xa6, 0x4f, 0x0c, 0xe2, 0x8d, 0xb9, 0x78, 0x26, 0x2b, 0x6f, 0xc3, 0x55, 0x4e, 0x63, 0x67, 0x1c, 0x53, 0x05, 0x4f, 0x98, 0xff, 0x2d, 0xd1, 0xc2, 0x54, 0xac, 0x5c, 0x33, 0x69, 0x86, 0xe2, 0x8b, 0x26, 0x88, 0x58, 0x74, 0x75, 0x9a, 0x13, 0xd2, 0xcc, 0x6e, 0x8d, 0x48, 0x55, 0xef, 0x31, 0x7d, 0x8c, 0xc8, 0xb1, 0x13, 0xc2, 0x64, 0xe6, 0xba, 0xf8, 0x80, 0x65, 0x53, 0x27, 0x2d, 0x96, 0x35, 0x84, 0x3e, 0x41, 0x7d, 0x5a, 0xb9, 0x55, 0xda, 0xc0, 0xc0, 0x69, 0x91, 0x22, 0xe2, 0xec, 0xa1, 0xaf, 0xdf, 0x84, 0x56, 0x42, 0x4a, 0xc1, 0x3b, 0xa8, 0xed, 0x8a, 0x62, 0xbc, 0xa7, 0xd2, 0x0a, 0xee, 0x05, 0x0f, 0xbc, 0x34, 0x96, 0x54, 0x3b, 0x40, 0x9d, 0x08, 0x46, 0x1d, 0xf4, 0xf1, 0x07, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbd, 0x0c, 0x54, 0x52, 0x6f, 0x61, 0xab, 0x2c, 0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa4, 0x81, 0x00, 0x00, 0x00, 0x00, 0x6d, 0x69, 0x6d, 0x65, 0x74, 0x79, 0x70, 0x65, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xeb, 0x5d, 0x5b, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0xed, 0x41, 0x3a, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x55, 0x54, 0x05, 0x00, 0x03, 0xb9, 0xb2, 0x39, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03, 0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0x29, 0x62, 0x5b, 0x52, 0xef, 0x71, 0x90, 0xc7, 0xa5, 0x00, 0x00, 0x00, 0x6f, 0x01, 0x00, 0x00, 0x2d, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa4, 0x81, 0x7d, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c, 0x65, 0x2e, 0x69, 0x62, 0x6f, 0x6f, 0x6b, 0x73, 0x2e, 0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x2d, 0x6f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2e, 0x78, 0x6d, 0x6c, 0x55, 0x54, 0x05, 0x00, 0x03, 0xbe, 0xb9, 0x39, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x50, 0x4b, 0x01, 0x02, 0x1e, 0x03, 0x14, 0x00, 0x02, 0x00, 0x08, 0x00, 0xce, 0x95, 0x54, 0x52, 0xaa, 0x1e, 0x0e, 0x69, 0x95, 0x00, 0x00, 0x00, 0xe4, 0x00, 0x00, 0x00, 0x16, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa4, 0x81, 0x89, 0x01, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x63, 0x6f, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x65, 0x72, 0x2e, 0x78, 0x6d, 0x6c, 0x55, 0x54, 0x05, 0x00, 0x03, 0x74, 0xda, 0x30, 0x60, 0x75, 0x78, 0x0b, 0x00, 0x01, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x04, 0xe8, 0x03, 0x00, 0x00, 0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00, 0xa6, 0x01, 0x00, 0x00, 0x3f, 0x03, 0x00, 0x00, 0x00, 0x00};

                char output[strlen(filename)+6];
                sprintf(output, "%s.epub",filename);
                FILE *epub_out;
                epub_out = fopen(output,"wb");
                fwrite(pre_ParanoidFFD_Template_epub,1275,1,epub_out);
                fclose(epub_out);

                // content.opf
                char contentopf[strlen(title) + strlen(argv[argvurl]) + 130*j + 855];
                char contentopf_manifest_chapters[90*j];
                char bufepub[90*j];
                char contentopf_spine_chapters[35*j];
                char bufepub2[35*j];
                int b = 1;
                sprintf(contentopf_manifest_chapters, "    <item id=\"chapter_%d\" href=\"chapter_%d.xhtml\" media-type=\"application/xhtml+xml\" />\n",b,b);
                sprintf(contentopf_spine_chapters, "    <itemref idref=\"chapter_%d\" />\n",b);
                b = b + 1;
                while (b <= j) {
                    sprintf(bufepub, "%s",contentopf_manifest_chapters);
                    sprintf(bufepub2, "%s",contentopf_spine_chapters);
                    sprintf(contentopf_manifest_chapters, "%s    <item id=\"chapter_%d\" href=\"chapter_%d.xhtml\" media-type=\"application/xhtml+xml\" />\n",bufepub,b,b);
                    sprintf(contentopf_spine_chapters, "%s    <itemref idref=\"chapter_%d\" />\n",bufepub2,b);
                    b = b + 1;
                }

                sprintf(contentopf, "zf.writestr(\"content.opf\", \"\"\"<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\" ?>\n<package xmlns=\\\"http://www.idpf.org/2007/opf\\\" xmlns:dc=\\\"http://purl.org/dc/elements/1.1/\\\" unique-identifier=\\\"db-id\\\" version=\\\"3.0\\\">\n\n<metadata>\n    <dc:title id=\\\"t1\\\">%s</dc:title>\n    <dc:identifier id=\\\"db-id\\\">%s-ParanoidFFD</dc:identifier>\n    <meta property=\\\"dcterms:modified\\\">%s</meta>\n    <dc:language>en</dc:language>\n</metadata>\n\n<manifest>\n    <item id=\\\"toc\\\" properties=\\\"nav\\\" href=\\\"toc.xhtml\\\" media-type=\\\"application/xhtml+xml\\\" />\n    <item id=\\\"ncx\\\" href=\\\"toc.ncx\\\" media-type=\\\"application/x-dtbncx+xml\\\" />\n    <item id=\\\"template_css\\\" href=\\\"template.css\\\" media-type=\\\"text/css\\\" />\n    <item id=\\\"cover\\\" href=\\\"cover.xhtml\\\" media-type=\\\"application/xhtml+xml\\\" />\n%s</manifest>\n\n<spine toc=\\\"ncx\\\">\n    <itemref idref=\\\"cover\\\" />\n%s</spine>\n\n</package>\"\"\")",title,argv[argvurl],datetime,contentopf_manifest_chapters,contentopf_spine_chapters);

                // toc.ncx
                char tocncx[strlen(title) + strlen(argv[argvurl]) + strlen(totalchapters) + 165*j + 490];
                char tocncx_navmap[strlen(totalchapters) + 165*j];
                char tocncxbuf[strlen(totalchapters) + 165*j];
                b = 1;
                sprintf(tocncx_navmap, "    <navPoint id=\\\"chapter_%d\\\" playOrder=\\\"%d\\\">\\n        <navLabel><text>%s</text></navLabel>\\n        <content src=\\\"chapter_%d.xhtml\\\" />\\n    </navPoint>\\n",b,b+1,chaptername[b],b);
                b = b + 1;
                while (b <= j) {
                    sprintf(tocncxbuf, "%s",tocncx_navmap);
                    sprintf(tocncx_navmap, "%s    <navPoint id=\\\"chapter_%d\\\" playOrder=\\\"%d\\\">\\n        <navLabel><text>%s</text></navLabel>\\n        <content src=\\\"chapter_%d.xhtml\\\" />\\n    </navPoint>\\n",tocncxbuf,b,b+1,chaptername[b],b);
                    b = b + 1;
                }

                sprintf(tocncx, "zf.writestr(\"toc.ncx\", \"\"\"<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\" ?>\n<ncx version=\\\"2005-1\\\" xml:lang=\\\"en\\\" xmlns=\\\"http://www.daisy.org/z3986/2005/ncx/\\\">\n\n<head>\n    <meta name=\\\"dtb:uid\\\" content=\\\"%s-ParanoidFFD\\\"/>\n    <meta name=\\\"dtb:depth\\\" content=\\\"1\\\"/>\n</head>\n\n<docTitle>\n    <text>toc</text>\n</docTitle>\n\n<navMap>\n    <navPoint id=\\\"cover\\\" playOrder=\\\"1\\\">\n        <navLabel><text>%s</text></navLabel>\n        <content src=\\\"cover.xhtml\\\" />\n    </navPoint>\n%s</navMap>\n\n</ncx>\"\"\")",argv[argvurl],title,tocncx_navmap);

                // toc.xhtml
                char tocxhtml[strlen(title) + strlen(totalchapters) + 60*j + 510];
                char toc_contents[strlen(totalchapters) + 60*j];
                char tocbuf[strlen(totalchapters) + 60*j];
                b = 1;
                sprintf(toc_contents, "            <li><a href=\\\"chapter_%d.xhtml\\\">%s</a></li>\\n",b,chaptername[b]);
                b = b + 1;
                while (b <= j) {
                    sprintf(tocbuf, "%s",toc_contents);
                    sprintf(toc_contents, "%s            <li><a href=\\\"chapter_%d.xhtml\\\">%s</a></li>\\n",tocbuf,b,chaptername[b]);
                    b = b + 1;
                }

                sprintf(tocxhtml, "zf.writestr(\"toc.xhtml\", \"\"\"<?xml version=\\\"1.0\\\" encoding=\\\"utf-8\\\"?>\n<html xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:epub=\\\"http://www.idpf.org/2007/ops\\\">\n<head>\n<title>toc</title>\n<link href=\\\"template.css\\\" rel=\\\"stylesheet\\\" type=\\\"text/css\\\" />\n</head>\n\n<body>\n\n    <nav id=\\\"toc\\\" epub:type=\\\"toc\\\">\n        <h1 class=\\\"frontmatter\\\">Table of Contents</h1>\n        <ol class=\\\"contents\\\">\n            <li><a href=\\\"cover.xhtml\\\">%s</a></li>\n%s        </ol>\n\n    </nav>\n\n</body>\n\n</html>\"\"\")",title,toc_contents);

                // cover.xhtml
                char cover[strlen(title)*j + strlen(author) + strlen(info) + strlen(summary) + strlen(argv[argvurl]) + 590];
                sprintf(cover, "zf.writestr(\"cover.xhtml\", \"\"\"<?xml version=\\\"1.0\\\" encoding=\\\"utf-8\\\"?>\n<html xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:epub=\\\"http://www.idpf.org/2007/ops\\\">\n<head>\n<title>%s</title>\n<link href=\\\"template.css\\\" rel=\\\"stylesheet\\\" type=\\\"text/css\\\" />\n</head>\n\n<body>\n\n    <h1><a href=\\\"%s\\\">%s</a></h1>\n    <h3>By %s</h3>\n    <br />\n    <pre>%sPublished %s\nUpdated %s\nDownloaded with <a href=\\\"https://github.com/Paranoid-Dev/ParanoidFFD\\\">ParanoidFFD</a>, %s</pre>\n    <br />\n    <p>%s</p>\n\n</body>\n</html>\"\"\")",title,argv[argvurl],title,author,info,published,updated,mark,summary);

                // write metadata to epub zip
                char writeoutput[strlen(output) + 75];
                sprintf(writeoutput, "zf = zipfile.ZipFile(\"%s\", mode=\"a\", compression=zipfile.ZIP_DEFLATED)",output);

                PyRun_SimpleString(writeoutput);
                PyRun_SimpleString(contentopf);
                PyRun_SimpleString(tocncx);
                PyRun_SimpleString(tocxhtml);
                PyRun_SimpleString(cover);

                int chapterlen = 0;
                for(int b2 = 1; b2 <= j; b2++) {
                    int need = (int)strlen(chaptername[b2])*2 + digits*2 + 370;
                    if(chapterlen < need) {
                        chapterlen = need;
                    }
                }

                char epubchapter[chapterlen];
                int b3 = 1;
                while (b3 <= j) {
                    sprintf(epubchapter, "zf.writestr(\"chapter_%d.xhtml\", \"\"\"<?xml version=\\\"1.0\\\" encoding=\\\"utf-8\\\"?>\n<html xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:epub=\\\"http://www.idpf.org/2007/ops\\\">\n<head>\n<title>%s</title>\n<link href=\\\"template.css\\\" rel=\\\"stylesheet\\\" type=\\\"text/css\\\" />\n</head>\n\n<body>\n\n    <h1>%s</h1>\n    <br />\n    \"\"\"f\"\"\"{chapters[%d]}\"\"\"\"\"\"\n</body>\n</html>\"\"\")",b3,chaptername[b3],chaptername[b3],b3-1);
                    PyRun_SimpleString(epubchapter);
                    b3 = b3 + 1;
                }

                PyRun_SimpleString("zf.close()");
                Py_Finalize();

                printf("Finished!\n");
            }
            Py_Finalize();
        }
    }
    return 0;
}
