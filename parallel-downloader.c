/*
   Paranoid FanFiction Downloader — modernized Selenium/EPUB
   Copyright 2021 Paranoid-Dev
   Modifications 2025 ChatGPT user-requested patch
   Licensed under the Apache License, Version 2.0
*/

#include <Python.h> // >= Python3.8
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

const char *title, *author, *summary, *info, *filename, *totalchapters, *url;
char updated[32];
char published[32];
char datetime_iso[22];
char mark[32];
time_t updatedunixtimestamp;
time_t publishedunixtimestamp;
PyObject *mainModule;

int p = 1;     // for argument parsing
int f = 1;     // 1=txt 2=epub
int fname = 1; // 1=use title as filename
int down = 0;
int argvurl = 0;
size_t l;     // buffer size for url assign
int j = 1;    // total chapters
int errnum = 0;
int t = 2;    // threads (logical; used by ThreadPoolExecutor)
int headful = 1;

void help() {
    puts(
        " ________________________________________________________________________________________ \n"
        "                 Paranoid FanFiction Downloader (modern)                                  \n"
        " ________________________________________________________________________________________ \n"
        " Usage    : ParanoidFFD <options>                                                         \n"
        " Options  :                                                                                \n"
        "   -u <url>            : download from <fanfiction url>                                   \n"
        "   -f <FORMAT>         : save as txt or epub (default txt)                                \n"
        "   -o <FILE_NAME>      : output file name (without extension)                             \n"
        "   -t <n>              : number of parallel download threads (default 2)                  \n"
        "   --headful           : run Chrome non-headless (default headless)                       \n"
        "   --version           : show version                                                     \n"
        "   --check-update      : check for updates (deprecated in this build)                     \n"
        "   -h , --help         : show help                                                        \n"
        " Examples :                                                                                \n"
        "   ParanoidFFD -u \"https://www.fanfiction.net/s/5435295/1/The-Bonds-of-Blood\"            \n"
        "   ParanoidFFD -f epub -t 4 -o \"My Fic\" -u \"https://www.fanfiction.net/s/ID/1/Title\"   \n"
        " ________________________________________________________________________________________ \n"
    );
}

int initializePy() {
    Py_Initialize();
    mainModule = PyImport_AddModule("__main__");

    // Embedded Python (modern Selenium + ThreadPoolExecutor)
    return PyRun_SimpleString(
        "import re, zipfile, base64, sys, time\n"
        "from concurrent.futures import ThreadPoolExecutor, as_completed\n"
        "from urllib.parse import urlparse\n"
        "from selenium import webdriver\n"
        "from selenium.webdriver.common.by import By\n"
        "from selenium.webdriver.support.ui import WebDriverWait\n"
        "from selenium.webdriver.support import expected_conditions as EC\n"
        "from selenium.common.exceptions import TimeoutException\n"
        "import html as html_mod\n"
        "\n"
        "def make_chrome(headless=True):\n"
        "    opts = webdriver.ChromeOptions()\n"
        "    if headless:\n"
        "        opts.add_argument('--headless=new')\n"
        "    opts.add_argument('--disable-blink-features=AutomationControlled')\n"
        "    opts.add_argument('--no-sandbox')\n"
        "    opts.add_argument('--disable-gpu')\n"
        "    opts.add_argument('--window-size=1200,1800')\n"
        "    return webdriver.Chrome(options=opts)\n"
        "\n"
        "def normalize_ffn_base(u: str) -> str:\n"
        "    if not u:\n"
        "        return ''\n"
        "    up = urlparse(u)\n"
        "    parts = [p for p in up.path.split('/') if p]\n"
        "    if len(parts) >= 2 and parts[0] == 's':\n"
        "        return f\"{up.scheme}://{up.netloc}/s/{parts[1]}/\"\n"
        "    return f\"{up.scheme}://{up.netloc}/s/\"\n"
        "\n"
        "def clean_chapter_label(s: str) -> str:\n"
        "    s = (s or '').strip()\n"
        "    s = re.sub(r'^\\s*\\d+\\.?\\s*', '', s)\n"
        "    return s or 'Chapter'\n"
        "\n"
        "def get_ffn_metadata(story_url: str, headless=True, timeout=20):\n"
        "    chrome = make_chrome(headless=headless)\n"
        "    try:\n"
        "        chrome.get(story_url)\n"
        "        wait = WebDriverWait(chrome, timeout)\n"
        "        wait.until(EC.presence_of_element_located((By.ID, 'profile_top')))\n"
        "        title_el = chrome.find_element(By.CSS_SELECTOR, '#profile_top b')\n"
        "        title = title_el.text.strip()\n"
        "        try:\n"
        "            author_el = chrome.find_element(By.XPATH, '//*[@id=\"profile_top\"]/a[1]')\n"
        "            author = author_el.text.strip()\n"
        "        except Exception:\n"
        "            author = 'Unknown'\n"
        "        try:\n"
        "            summary_el = chrome.find_element(By.CSS_SELECTOR, '#profile_top > div')\n"
        "            summary = summary_el.text.strip()\n"
        "        except Exception:\n"
        "            summary = ''\n"
        "        try:\n"
        "            info_el = chrome.find_element(By.XPATH, '//*[@id=\"profile_top\"]/span[4]')\n"
        "        except Exception:\n"
        "            try:\n"
        "                info_el = chrome.find_element(By.XPATH, '//*[@id=\"profile_top\"]/span[3]')\n"
        "            except Exception:\n"
        "                info_el = None\n"
        "        info = (info_el.text.strip() if info_el else '')\n"
        "        upd_txt, pub_txt = '', ''\n"
        "        try:\n"
        "            upd_span = chrome.find_element(By.XPATH, '//*[@id=\"profile_top\"]/span/span[1]')\n"
        "            upd_txt = upd_span.get_attribute('data-xutime') or ''\n"
        "        except Exception:\n"
        "            pass\n"
        "        try:\n"
        "            pub_span = chrome.find_element(By.XPATH, '//*[@id=\"profile_top\"]/span/span[2]')\n"
        "            pub_txt = pub_span.get_attribute('data-xutime') or ''\n"
        "        except Exception:\n"
        "            if not pub_txt:\n"
        "                pub_txt = upd_txt\n"
        "        opts = chrome.find_elements(By.CSS_SELECTOR, '#chap_select option')\n"
        "        chapters_by_num = {}\n"
        "        for o in opts:\n"
        "            val = (o.get_attribute('value') or '').strip()\n"
        "            m = re.search(r'(\\d+)', val)\n"
        "            if not m:\n"
        "                continue\n"
        "            num = int(m.group(1))\n"
        "            if num not in chapters_by_num:\n"
        "                txt = (o.text or o.get_attribute('textContent') or '').strip()\n"
        "                chapters_by_num[num] = clean_chapter_label(txt)\n"
        "        if not chapters_by_num:\n"
        "            chapter_names = [title or 'Chapter 1']\n"
        "        else:\n"
        "            chapter_names = [chapters_by_num[n] for n in sorted(chapters_by_num)]\n"
        "        base = normalize_ffn_base(story_url)\n"
        "        return {\n"
        "            'title': title,\n"
        "            'author': author,\n"
        "            'summary': summary,\n"
        "            'info': info,\n"
        "            'published_unix': int(pub_txt) if pub_txt.isdigit() else None,\n"
        "            'updated_unix': int(upd_txt) if upd_txt.isdigit() else None,\n"
        "            'chapter_names': chapter_names,\n"
        "            'base_url': base,\n"
        "        }\n"
        "    finally:\n"
        "        try:\n"
        "            chrome.quit()\n"
        "        except Exception:\n"
        "            pass\n"
        "\n"
        "def fetch_chapter(url: str, headless=True, timeout=25, retries=3):\n"
        "    for attempt in range(1, retries+1):\n"
        "        chrome = make_chrome(headless=headless)\n"
        "        try:\n"
        "            print(f\"Downloading [ {url} ]...\")\n"
        "            chrome.get(url)\n"
        "            WebDriverWait(chrome, timeout).until(\n"
        "                EC.presence_of_element_located((By.ID, 'storytext'))\n"
        "            )\n"
        "            html_el = chrome.find_element(By.ID, 'storytext')\n"
        "            chapter = html_el.get_attribute('innerHTML') or ''\n"
        "            chapter = re.sub(r'<div.*?</div>', '', chapter, flags=re.DOTALL)\n"
        "            chapter = re.sub(r'(<hr|<br)\\b([^>]*)>', r'\\1\\2 />', chapter, flags=re.DOTALL)\n"
        "            print(f\"Finished downloading [ {url} ]\")\n"
        "            return chapter\n"
        "        except Exception as e:\n"
        "            print(f\"Failed to download {url} — {e}\\n Retrying in 15s...\")\n"
        "            time.sleep(15)\n"
        "        finally:\n"
        "            try:\n"
        "                chrome.quit()\n"
        "            except Exception:\n"
        "                pass\n"
        "    return f\"Failed to download {url}\"\n"
    );
}

void Py_Launcher() {
    while (initializePy()) {
        ++errnum;
        if (errnum == 5) {
            puts("\nParanoidFFD Python failed to initialize");
            Py_Finalize();
            exit(2);
        }
        puts("\nParanoidFFD Python initialization error - Trying again..\n");
        Py_Finalize();
    }
    errnum = 0;
    puts("ParanoidFFD Py initialized");
}

const char * HTML_SpecialChar_encode (const char* HTML) {
    char HTMLbuf[strlen(HTML)+80];
    sprintf(HTMLbuf, "out = html_mod.escape(\"\"\"%s\"\"\")", HTML);
    PyRun_SimpleString(HTMLbuf);
    PyObject *HTMLencodedPy = PyObject_GetAttrString(mainModule, "out");
    return PyUnicode_AsUTF8(HTMLencodedPy);
}

void fic_info () {
    time_t now;
    time(&now);
    struct tm* now_tm = gmtime(&now);
    strftime (datetime_iso, 22, "%FT%TZ", now_tm);
    strftime (mark, 32, "on %F, at %T", now_tm);

    puts("Parsed fanfiction url\nFetching fanfiction info ...");

    PyRun_SimpleString("headful_flag = False");
    if (headful) PyRun_SimpleString("headful_flag = True");

    PyRun_SimpleString(
        "meta = None\n"
        "try:\n"
        "    meta = get_ffn_metadata(url, headless=not headful_flag)\n"
        "except Exception as e:\n"
        "    print('Failed to get metadata', e)\n"
    );

    PyObject *meta = PyObject_GetAttrString(mainModule, "meta");
    if (!meta || meta == Py_None) {
        puts("Failed to get metadata");
        Py_Finalize();
        exit(4);
    }

    PyObject *py_title   = PyDict_GetItemString(meta, "title");
    PyObject *py_author  = PyDict_GetItemString(meta, "author");
    PyObject *py_summary = PyDict_GetItemString(meta, "summary");
    PyObject *py_info    = PyDict_GetItemString(meta, "info");
    PyObject *py_base    = PyDict_GetItemString(meta, "base_url");
    PyObject *py_pub     = PyDict_GetItemString(meta, "published_unix");
    PyObject *py_upd     = PyDict_GetItemString(meta, "updated_unix");

    title   = PyUnicode_AsUTF8(py_title);
    author  = PyUnicode_AsUTF8(py_author);
    summary = PyUnicode_AsUTF8(py_summary);
    info    = PyUnicode_AsUTF8(py_info);
    url     = PyUnicode_AsUTF8(py_base);

    if (py_upd && py_upd != Py_None) {
        updatedunixtimestamp = (time_t) PyLong_AsLong(py_upd);
        struct tm *updatedtimestamp = gmtime(&updatedunixtimestamp);
        strftime (updated, 32, "on %F, at %T", updatedtimestamp);
    } else { strcpy(updated, ""); }

    if (py_pub && py_pub != Py_None) {
        publishedunixtimestamp = (time_t) PyLong_AsLong(py_pub);
        struct tm *publishedtimestamp = gmtime(&publishedunixtimestamp);
        strftime (published, 32, "on %F, at %T", publishedtimestamp);
    } else { strcpy(published, ""); }

    PyRun_SimpleString("chapterlist = '\\n'.join(meta['chapter_names'])");
    PyObject *chapterlistPy = PyObject_GetAttrString(mainModule, "chapterlist");
    totalchapters = PyUnicode_AsUTF8(chapterlistPy);

    puts("Fanfiction info downloaded\nParsing chapters ...");

    const char * chapterlist = PyUnicode_AsUTF8(chapterlistPy);
    j = 0;
    for (const char *s = chapterlist; *s; ++s) {
        if (*s == '\n') ++j;
    }
    if (strlen(chapterlist) > 0) ++j; // last line if not newline-terminated
}

int main (int argc, char *argv[]) {
    if (argc == 1) {
        puts(
            " ________________________________________________________________________________________ \n"
            "                 Paranoid FanFiction Downloader (modern)                                  \n"
            " ________________________________________________________________________________________ \n"
            " \"ParanoidFFD --help\" to show help page                                                   \n"
        );
        return 0;
    }

    while (p < argc) {
        if (strcmp(argv[p], "--version") == 0) {
            puts("ParanoidFFD modern 1.4.1");
        }
        else if (strcmp(argv[p], "--help") == 0 || strcmp(argv[p], "-h") == 0) {
            help();
        }
        else if (strcmp(argv[p], "-t") == 0) {
            ++p; if (p>=argc) { puts("missing -t value"); exit(5); }
            t = atoi(argv[p]);
            if (t < 1) { printf("invalid thread count : %s\n", argv[p]); exit(5); }
        }
        else if (strcmp(argv[p], "--headful") == 0) {
            headful = 1;
        }
        else if (strcmp(argv[p], "-f") == 0) {
            ++p; if (p>=argc) { puts("missing -f value"); exit(5); }
            if (strcmp(argv[p], "txt") == 0) f = 1;
            else if (strcmp(argv[p], "epub") == 0) f = 2;
            else { printf("invalid format : %s\n", argv[p]); exit(5); }
        }
        else if (strcmp(argv[p], "-o") == 0) {
            ++p; if (p>=argc) { puts("missing -o value"); exit(5); }
            fname = 0; filename = argv[p];
        }
        else if (strcmp(argv[p], "-u") == 0) {
            ++p; if (p>=argc) { puts("missing -u url"); exit(5); }
            argvurl = p; down = 1; l = strlen(argv[p]) + 32;
        }
        else if (strcmp(argv[p], "--check-update") == 0) {
            puts("ParanoidFFD update checker deprecated in modern build.");
        }
        else {
            // Ignore unknown flags starting with '-' (e.g., Python-internal -c)
            if (argv[p][0] == '-') {
                // ignore silently
            } else {
                printf("invalid argument : %s \n", argv[p]);
                exit(5);
            }
        }
        ++p;
    }

    if (!down) return 0;

    printf("Downloading %s with %d threads%s...\n",
           argv[argvurl], t, headful ? " (headful)" : "");

    Py_Launcher();

    // Normalize input URL & push into Python 'url'
    char buf[2048];
    snprintf(buf, sizeof(buf), "url = \"%s\"", argv[argvurl]);
    PyRun_SimpleString(buf);
    PyRun_SimpleString("url = normalize_ffn_base(url)"); // story base

    fic_info();

    // Build C array of chapter names from Python meta
    PyObject *meta = PyObject_GetAttrString(mainModule, "meta");
    PyObject *py_chapters = PyDict_GetItemString(meta, "chapter_names");
    int chapter_count = (int)PyList_Size(py_chapters);
    if (chapter_count < 1) chapter_count = 1;

    char **chaptername = (char**)calloc(chapter_count+1, sizeof(char*));
    for (int i = 0; i < chapter_count; ++i) {
        PyObject *item = PyList_GetItem(py_chapters, i);
        const char *nm = PyUnicode_AsUTF8(item);
        chaptername[i+1] = (char*)calloc(strlen(nm)+1, 1);
        strcpy(chaptername[i+1], nm);
    }
    j = chapter_count;

    for (int i = 1; i <= j; ++i) {
        printf("[ %d. %s ]\n", i, chaptername[i]);
    }
    puts("Parsed chapters");

    if (fname) filename = title; // default filename = title

    // Build URLs strictly up to j
    PyRun_SimpleString("urls = []");
    for (int i = 1; i <= j; ++i) {
        char addurl[1024];
        snprintf(addurl, sizeof(addurl),
                 "urls.append(f\"%s%d\")", url, i);
        PyRun_SimpleString(addurl);
    }

    char threadPy[64];
    snprintf(threadPy, sizeof(threadPy), "thread_count = %d", t);
    PyRun_SimpleString(threadPy);
    PyRun_SimpleString("headless_flag = True");
    if (headful) PyRun_SimpleString("headless_flag = False");

    // ThreadPoolExecutor download, avoiding 'html' name
    PyRun_SimpleString(
        "chapters = [None]*len(urls)\n"
        "def _fetch_indexed(i_url):\n"
        "    i, u = i_url\n"
        "    ch_html = fetch_chapter(u, headless=headless_flag)\n"
        "    return (i, ch_html)\n"
        "with ThreadPoolExecutor(max_workers=thread_count) as ex:\n"
        "    futs = [ex.submit(_fetch_indexed, (i, u)) for i, u in enumerate(urls)]\n"
        "    for fut in as_completed(futs):\n"
        "        i, ch_html = fut.result()\n"
        "        chapters[i] = ch_html\n"
    );

    puts("Download finished\nWriting to file ...");

    if (f == 1) {
        char output[strlen(filename)+5];
        sprintf(output, "%s.txt", filename);
        FILE *fp = fopen(output, "w");
        if (!fp) { perror("open txt"); exit(6); }

        fprintf(fp, "%s\nBy %s\n\n%s", title, author, info && *info ? info : "");
        if (*published) fprintf(fp, "Published %s\n", published);
        if (*updated)   fprintf(fp, "Updated %s\n", updated);
        fprintf(fp, "Downloaded with ParanoidFFD, %s\n\n", mark);
        if (summary && *summary) fprintf(fp, "%s\n\n", summary);

        fprintf(fp, "Chapters\n\n");
        for (int i = 1; i <= j; ++i) fprintf(fp, "%s\n", chaptername[i]);
        fprintf(fp, "\n");

        for (int b = 0; b < j; ++b) {
            char sel[64];
            snprintf(sel, sizeof(sel), "out = chapters[%d]", b);
            PyRun_SimpleString(sel);
            PyRun_SimpleString(
                "out = re.sub('</p>','\\n\\n', out or '', flags=re.DOTALL)\n"
                "out = re.sub('<p.*?>','', out, flags=re.DOTALL)\n"
                "out = re.sub('<br.*?/>','\\n', out, flags=re.DOTALL)\n"
                "out = re.sub('<hr.*?/>','\\n- — - — - — -\\n', out, flags=re.DOTALL)\n"
                "out = re.sub('<.*?>','', out, flags=re.DOTALL)\n"
                "out = html_mod.unescape(out)\n"
            );
            PyObject *chapterPy = PyObject_GetAttrString(mainModule, "out");
            const char *chapterText = PyUnicode_AsUTF8(chapterPy);

            fprintf(fp, "| %s |\n\n", chaptername[b+1]);
            fprintf(fp, "%s\n\n", chapterText ? chapterText : "");
        }
        fclose(fp);
        printf("Saving to \"%s\"\nFinished!\n", output);
    }
    else if (f == 2) {
        char output[strlen(filename)+6];
        sprintf(output, "%s.epub", filename);
        FILE *epub_out = fopen(output, "wb");
        if (!epub_out) { perror("open epub"); exit(6); }
        fclose(epub_out);

        const char *title_html   = HTML_SpecialChar_encode(title);
        const char *author_html  = HTML_SpecialChar_encode(author);
        const char *summary_html = HTML_SpecialChar_encode(summary ? summary : "");
        const char *info_html    = HTML_SpecialChar_encode(info ? info : "");
        const char *url_html     = HTML_SpecialChar_encode(url);

        PyRun_SimpleString("from datetime import datetime, timezone\n");

        {
            char pyset[8192];
            snprintf(pyset, sizeof(pyset),
                "epub_path = r'''%s'''\n"
                "t_title = r'''%s'''\n"
                "t_author = r'''%s'''\n"
                "t_summary = r'''%s'''\n"
                "t_info = r'''%s'''\n"
                "t_url = r'''%s'''\n"
                "dt_now = r'''%s'''\n",
                output, title_html, author_html, summary_html, info_html, url_html, datetime_iso
            );
            PyRun_SimpleString(pyset);
        }

        PyRun_SimpleString("chap_names = []");
        for (int i = 1; i <= j; ++i) {
            const char *nm_html = HTML_SpecialChar_encode(chaptername[i]);
            char add[4096];
            snprintf(add, sizeof(add), "chap_names.append(r'''%s''')", nm_html);
            PyRun_SimpleString(add);
        }

        PyRun_SimpleString(
            "def write_epub(epub_path, title, author, summary, info, url, chap_names, chapters, iso_dt):\n"
            "    with open(epub_path, 'wb') as _:\n"
            "        pass\n"
            "    zf = zipfile.ZipFile(epub_path, mode='a', compression=zipfile.ZIP_STORED)\n"
            "    zf.writestr('mimetype', 'application/epub+zip')\n"
            "    zf.close()\n"
            "    zf = zipfile.ZipFile(epub_path, mode='a', compression=zipfile.ZIP_DEFLATED)\n"
            "    zf.writestr('META-INF/container.xml', (\n"
            "        '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\\n'\n"
            "        '<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\\n'\n"
            "        '  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>\\n'\n"
            "        '</container>'\n"
            "    ))\n"
            "    css = (\n"
            "        'body{font-family: -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial;'\n"
            "        'line-height:1.5;margin:1.2em;}\\n'\n"
            "        'h1,h2{font-weight:700;margin:0.6em 0 0.3em 0;}\\n'\n"
            "        '.frontmatter{font-size:1.4em;margin:0.5em 0;}\\n'\n"
            "        '.meta{color:#555;white-space:pre-wrap}\\n'\n"
            "        '.chapter-title{margin-top:0.2em;border-bottom:1px solid #999;padding-bottom:0.2em;}\\n'\n"
            "        'p{text-align:justify;}\\n'\n"
            "        'hr{border:none;border-top:1px solid #bbb;margin:1em 0;}'\n"
            "    )\n"
            "    zf.writestr('OEBPS/template.css', css)\n"
            "    xhtmls = []\n"
            "    for i, (name, html_frag) in enumerate(zip(chap_names, chapters), start=1):\n"
            "        body = (html_frag or '')\n"
            "        page = (\n"
            "            '<?xml version=\"1.0\" encoding=\"utf-8\"?>\\n'\n"
            "            '<html xmlns=\"http://www.w3.org/1999/xhtml\">\\n<head>\\n'\n"
            "            f'<title>{name}</title>\\n'\n"
            "            '<link rel=\"stylesheet\" type=\"text/css\" href=\"template.css\"/>\\n'\n"
            "            '</head>\\n<body>\\n'\n"
            "            f'<h1 class=\"chapter-title\">{name}</h1>\\n'\n"
            "            f'{body}\\n'\n"
            "            '</body>\\n</html>'\n"
            "        )\n"
            "        fn = f'OEBPS/chapter_{i}.xhtml'\n"
            "        zf.writestr(fn, page)\n"
            "        xhtmls.append(fn)\n"
            "    nav_items = '\\n'.join(\n"
            "        f'      <li><a href=\"chapter_{i+1}.xhtml\">{html_mod.escape(n)}</a></li>'\n"
            "        for i, n in enumerate(chap_names)\n"
            "    )\n"
            "    nav = (\n"
            "        '<?xml version=\"1.0\" encoding=\"utf-8\"?>\\n'\n"
            "        '<!DOCTYPE html>\\n<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\\n'\n"
            "        '<head><title>Table of Contents</title><link rel=\"stylesheet\" type=\"text/css\" href=\"template.css\"/></head>\\n'\n"
            "        '<body>\\n<nav epub:type=\"toc\" id=\"toc\"><h1 class=\"frontmatter\">Table of Contents</h1>\\n<ol>\\n'\n"
            "        f'{nav_items}\\n'\n"
            "        '</ol></nav>\\n</body></html>'\n"
            "    )\n"
            "    zf.writestr('OEBPS/toc.xhtml', nav)\n"
            "    cover = (\n"
            "        '<?xml version=\"1.0\" encoding=\"utf-8\"?>\\n<html xmlns=\"http://www.w3.org/1999/xhtml\">\\n<head>\\n'\n"
            "        f'<title>{title}</title>\\n<link rel=\"stylesheet\" type=\"text/css\" href=\"template.css\"/>\\n'\n"
            "        '</head>\\n<body>\\n'\n"
            "        f'<h1>{title}</h1>\\n<h3>By {author}</h3>\\n'\n"
            "        f'<div class=\"meta\">{info}\\nURL: {url}\\nDownloaded {iso_dt}</div>\\n'\n"
            "        f'<p>{summary}</p>\\n'\n"
            "        '</body>\\n</html>'\n"
            "    )\n"
            "    zf.writestr('OEBPS/cover.xhtml', cover)\n"
            "    manifest_items = ['    <item id=\"nav\" href=\"toc.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/>',\n"
            "                     '    <item id=\"css\" href=\"template.css\" media-type=\"text/css\"/>']\n"
            "    spine_items = ['    <itemref idref=\"cover\"/>']\n"
            "    for i in range(1, len(chap_names)+1):\n"
            "        manifest_items.append(f'    <item id=\"chapter_{i}\" href=\"chapter_{i}.xhtml\" media-type=\"application/xhtml+xml\"/>')\n"
            "        spine_items.append(f'    <itemref idref=\"chapter_{i}\"/>')\n"
            "    opf = (\n"
            "        '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\\n'\n"
            "        '<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"book-id\">\\n'\n"
            "        '  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\\n'\n"
            "        f'    <dc:title>{title}</dc:title>\\n'\n"
            "        f'    <dc:creator>{author}</dc:creator>\\n'\n"
            "        '    <dc:language>en</dc:language>\\n'\n"
            "        f'    <dc:identifier id=\"book-id\">{url}ParanoidFFD</dc:identifier>\\n'\n"
            "        f'    <meta property=\"dcterms:modified\">{iso_dt}</meta>\\n'\n"
            "        '  </metadata>\\n'\n"
            "        '  <manifest>\\n'\n"
            "        '    <item id=\"cover\" href=\"cover.xhtml\" media-type=\"application/xhtml+xml\"/>\\n'\n"
            "        f\"{chr(10).join(manifest_items)}\\n\"\n"
            "        '  </manifest>\\n'\n"
            "        '  <spine>\\n'\n"
            "        f\"{chr(10).join(spine_items)}\\n\"\n"
            "        '  </spine>\\n'\n"
            "        '</package>'\n"
            "    )\n"
            "    zf.writestr('OEBPS/content.opf', opf)\n"
            "    zf.close()\n"
            "\n"
            "write_epub(epub_path, t_title, t_author, t_summary, t_info, t_url, chap_names, chapters, dt_now)\n"
        );

        printf("Saving to \"%s\"\nFinished!\n", output);
    }

    for (int i = 1; i <= j; ++i) free(chaptername[i]);
    free(chaptername);
    Py_Finalize();
    return 0;
}
