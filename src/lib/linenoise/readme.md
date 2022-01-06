# Linenoise

A line-editing library by Salvatore Sanfilippo.

* Source: https://github.com/antirez/linenoise
* Downloaded: 2021-11-25
* Last commit date: 2020-03-12
* Last commit hash: 97d2850af13c339369093b78abe5265845d78220

Changes:

* Edited to eliminate a compiler warning about an unused debugging macro.
* Added a POSIX define to enable compiler support for `strdup()` and `strcasecmp()`.
* Added a `<strings.h>` include for `strcasecmp()`.
