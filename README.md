# fileno(3) on C++ Streams: A Hacker's Lament

There is a need to bridge the gap between high-level programming concepts and low-level operating system facilities. On Unix, we occasionally need to directly access the file descriptor pertaining to a C++ I/O stream. This, however, is much more complicated than it should be. I try to provide a partial solution.

## Motivation

Have you ever mused about the little miracles that constantly happen to you on the Unix shell? Like this one:

```
funbox:~$ ls
fileno.cpp  fileno.hpp  main.cpp  Makefile  public_html/  stuff/
funbox:~$ ls |wc -l
6
```

First, <code>ls(1)</code> prints one line listing six items in my home directory. But when I pipe it into <code>wc(1)</code> with the <code>-l</code> switch, which is supposed to count the number of lines, it tells me there are six lines. Which is fine, since what I actually wanted to do was count the number of files and directories, right? But still, it all started with one line.

Or what about this one:

```
funbox:~$ last |wc
320
funbox:~$ last |more |wc
320
```

Wait! Shouldn't the <code>more(1)</code> command in the last line have done something funny with the terminal? And introduced lines saying ***--More--*** awaiting a keystroke from the user? What happened to it? Or, now that we seem to be onto something: What happens when you pipe the output of <code>more(1)</code> into <code>less(1)</code>, and that into <code>more(1)</code> again? Try it! Oh, and what happens to the formatting when you just redirect the output of <code>less(1)</code> to a file?

Perplexed? The secret is that all these little Unix utilities have a way of knowing whether they are writing/reading to/from a terminal or to/from something else and they adjust their behavior accordingly. It turns out there is a C library function <code>isatty(3)</code> which tests whether its argument, an open file descriptor, is associated with a terminal device. And yes, this also works with input streams like 0, the file descriptor associated with input from the command line by default. After all, the user experience of a shell script piping canned input into a program is vastly different from the user experience of a human typing at the same program. To sum up:

***Programs with a well-written command line interface discriminate among their audience.***

Imagine how confusing it would be if you couldn't just count the number of items in a directory with <code>ls |wc</code>. You would always have to say <code>ls -l |wc</code>. And you would probably forget it from time to time. Most people I've asked take the behavior in the first example above as granted and haven't ever even thought about why it actually works! Not all programs get it right, though. Here is an example what happens when an interpreter does not discriminate among its audience. Oracle's lame SQLplus is an interpreter which unconditionally emits annoying prompts (and other rubbish):

```
funbox:~$ ORACLE_SID=musicdb sqlplus / < insertscript.sql
SQL*Plus Release 9.2.0.1.0 - Production on Sun Feb 13 10:51:37 2005

Copyright (c) 1982, 2002, Oracle Corporation.  All rights reserved.

Connected to:
Oracle9i Enterprise Edition Release 9.2.0.1.0 -Production
JServer Release 9.2.0.1.0 - Production
SQL> SQL> SQL>    2    3    4    5    6    7    8    9    10    11
1 row created.

SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL> SQL>    2    3    4
ARTIST     ALBUM
---------- ------------------------------
Pink Floyd The Wall
Pink Floyd The Piper at the Gates of Dawn
Pink Floyd Ummagumma
Pink Floyd The Dark Side of the Moon
```

This, and its absent command line editing facilities has even led rebels to hack wrappers around SQLPlus. Wolfram's Mathematica also has such a silly interface. Here I obtain 5 gratuitous prompts while running a simple benchmark (never mind the actual benchmark code):

```
funbox:~$ cat dennyfliegner.ma
size:=200
A:=Array[a, 200]
e:=Sum[a[i],{i,1,size}]
aux:=-e+a[1]+a[2]
Timing[Expand[Expand[e^2] /. a[1]->aux]]
funbox:~$ cat dennyfliegner.ma |math
Mathematica 5.0 for Linux
Copyright 1988-2003 Wolfram Research, Inc.
 -- Terminal graphics initialized --

In[1]:=
In[2]:=
In[3]:=
In[4]:=
In[5]:=
                             2
Out[5]= {2.88056 Second, a[2] }

In[6]:=
funbox:~$
```

There are hopelessly many programs that don't care, sporting a cold, unempathic feel. They seem to prevail in the closed-source world, presumably because in an open-source project people would submit the (mostly trivial) patches to fix it long before the product leaves beta status. The GNU Coding Standards, while warning against disruptive changes in the behavior of a program depending on the output device, encourages this distinction.

Curious how you can apply all this to your own programs? Well, in Unix/C you are either dealing with integer file descriptors or with file streams <code>FILE\*</code>. File descriptors are per-process abstractions. They are the things your operating system provides. In most cases they are created with <code>open(2)</code> and closed with <code>close(2)</code>. A FILE\* is a higher level abstraction provided by the C language. You create one with <code>fopen(3)</code> and later throw it away with <code>fclose(3)</code>. The C standard library provides some streams for your convenience: <code>stdin</code>, <code>stdout</code>, and <code>stderr</code>, corresponding to the pre-defined file descriptors 0, 1, and 2, respectively. On Unix, such a file stream adds buffering to the raw file descriptors. For this reason, a <code>FILE\*</code> is somewhat more convenient to deal with than a raw file descriptor. But, since the sweet <code>isatty(3)</code> library function (and several hundred other extremely useful library functions) only works on the lower level file descriptors, you cannot stick to <code>FILE\*</code>. You need to know about the underlying file descriptor. The file stream abstraction leaks. Luckily, there is a C function that maps a stream pointer (<code>FILE\*</code>) to a file descriptor (int): It is called <code>fileno(3)</code>. You can now sense where your input comes from and where your output goes to, even if you are working with FILE\* streams almost all of the time. *Voilà!*

## Enter C++

Just as the C standard does not talk about file descriptors (because they are specific to Unix and C strives for portability across operating systems), the C++ standard does not talk about file descriptors either. Standard C has the <code>FILE\*</code> abstraction, standard C++ has streams, stream iterators, stream buffers, manipulators and whatnot. It is a wonderful architecture with a level of theoretical flexibility not found in C. Still, on a Unix system, when we do real I/O, there is always going to be a good old file descriptor under the stream's hood. That's a direct consequence of the fact that all I/O is ultimately performed by the operating system.

However, what happens when we do want to adjust the formatting of output depending on whether it goes to a terminal in order to be read by a human being or it is picked up by another program? For extra difficulty, let's assume we are a library implementor and our interface dictates that all we get is a reference to an output stream and that's it. Our client is determined to use <code><iostream></code> and nothing else, end of story. Can we tell what kind of stream we are writing to? Well, we can test if it is an ostringstream by using <code>dynamic_cast<></code>. Similarly, we can test if it is an <code>ofstream</code>. But then what? Well, maybe we could just guess it: If its address is the same as the address of cout, we can assume that it is the file descriptor number 1 and so on. But, can we really rely on these well-known numbers? Maybe we can, in most cases. But certainly not in all cases, since theoretically the application might have done strange things to cout and friends. And that leaves open the question what to assume when the stream we received is some entirely new stream? Can we assume then, that there is only one terminal attached to a process so that all other streams are necessarily files? Let's stop this insanity! (Anyway, the answer is no.)

That is not a theoretical scenario. I once found myself having to implement a single-line editor *à la* GNU readline. The application would only pass me a reference to an input stream where to read from and a reference to an output stream, to be used for echoing, if that made sense. How could I, as the designer of the library, possibly tell whether the application was called like app>/dev/null (echoing does not make sense), or like cat file|app (single-line editing does not make sense) or just like app (aha, that's where single-line editing makes sense)? The only reasonable way is to figure that from the two streams themselves!

So there we are. We have a nice library function <code>isatty(3)</code> that could get the job done but it is of no use because there is no safe way to find out which file descriptor belongs to which stream. Let this sink in:

Given a C++ stream, there is no general way to find out its file descriptor.
From this follows that the little tricks that most common Unix tools do could not be done were they written in C++. More generally, we might even have to do more adventurous things to file descriptors than finding out if they are attached to a terminal. Like <code>mmap(2)</code>ing them for instance, which might be rather off-limits because the stream has no way to track what we do to the underlying file descriptor. Or, as another example, we might need to set up the streams of a child process. The description of <code>fileno(3)</code> in the POSIX standard explicitly mentions this in its rationale section. If we want to find out whether there is input pending on a stream without risking danger of blocking, we'll also have to resort to low-level functions like <code>select(2)</code>. (But beware of input waiting in the buffers!) Last but not least, imagine a GUI toolkit that couldn't access the X-server's descriptor using <code>((struct _XDisplay*)display)->fd</code> or similar. Certain things simply cannot be done without access to the file descriptor.

So, yes, exposing the file descriptor opens a Pandora's box of exciting new ways to shoot ourselves in the foot. But you know what? So does the <code>fileno(3)</code> call! By bridging the gap between C's <code>FILE\*</code> streams and the underlying file descriptor it suddenly became possible to corrupt the streams for good. This ill-fortuned program tries to read from such a corrupted stream:

```C
#include <unistd.h>
#include <stdio.h>

int main()
{
    char buf[4];
    FILE\* f = fopen("/dev/zero", "r");
    close(fileno(f));  /* Evil: closing f's underlying file descriptor! */
    fread(buf, 1, 4, f);  /* Ouch: Try, read four bytes into buf.*/
    printf("%c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);
    return 0;
}
```

In the C world, nobody really cares that much. One reason is that the line containing <code>fread(buf, 1, 4, f)</code> is buggy anyway. It really should check how many items were read by evaluating the return value of <code>fread(3)</code>. Another reason is sheer pragmatism: Instead of disallowing us to shoot ourselves in the foot they just warn us.

The dilemma is enough to drive one to despair: We cannot reliably know what the file descriptor is when we want/have to make use of the standard C++ I/O library. But, at least on Unix systems, the file descriptor is the fundamental abstraction the kernel is using! The I/O library resembles a judiciously architectured, elegant building with the entrance to the basement mysteriously hidden away from us residents.

## A Solution

What did POSIX do in order to give us access to a C stream's file descriptor? They gave us <code>fileno(3)</code> so that we can leverage the bogillions of functions that expect a file descriptor as one of their arguments. Let's just do the same and write a free function that takes a C++ stream by reference and returns its integer file descriptor. The header file could be as simple as this:

```C++
#include <iosfwd>

template <typename charT, typename traits>
int fileno(const std::basic_ios<charT, traits>& stream);
```

The implementation has to hide away all the compiler-dependent intricacies. It turns out that, at least for GCC, the mechanism how to fetch the file descriptor is very sensitive to the version. In one case it even depends on the patchlevel. Okay, now be sure to fasten your seatbelts. Here is the code:

```C++
#include "fileno.hpp"
#include <cstdio>  // declaration of ::fileno
#include <fstream>  // for basic_filebuf template
#include <cerrno>

#if defined(__GLIBCXX__) || (defined(__GLIBCPP__) && __GLIBCPP__>=20020514)  // GCC >= 3.1.0
# include <ext/stdio_filebuf.h>
#endif
#if defined(__GLIBCXX__) // GCC >= 3.4.0
# include <ext/stdio_sync_filebuf.h>
#endif

//! Similar to fileno(3), but taking a C++ stream as argument instead of a
//! FILE*.  Note that there is no way for the library to track what you do with
//! the descriptor, so be careful.
//! \return  The integer file descriptor associated with the stream, or -1 if
//!   that stream is invalid.  In the latter case, for the sake of keeping the
//!   code as similar to fileno(3), errno is set to EBADF.
//! \see  The <A HREF="http://www.ginac.de/~kreckel/fileno/">upstream page at
//!   http://www.ginac.de/~kreckel/fileno/</A> of this code provides more
//!   detailed information.
template <typename charT, typename traits>
inline int
fileno_hack(const std::basic_ios<charT, traits>& stream)
{
    // Some C++ runtime libraries shipped with ancient GCC, Sun Pro,
    // Sun WS/Forte 5/6, Compaq C++ supported non-standard file descriptor
    // access basic_filebuf<>::fd().  Alas, starting from GCC 3.1, the GNU C++
    // runtime removes all non-standard std::filebuf methods and provides an
    // extension template class __gnu_cxx::stdio_filebuf on all systems where
    // that appears to make sense (i.e. at least all Unix systems).  Starting
    // from GCC 3.4, there is an __gnu_cxx::stdio_sync_filebuf, in addition.
    // Sorry, darling, I must get brutal to fetch the darn file descriptor!
    // Please complain to your compiler/libstdc++ vendor...
#if defined(__GLIBCXX__) || defined(__GLIBCPP__)
    // OK, stop reading here, because it's getting obscene.  Cross fingers!
# if defined(__GLIBCXX__)  // >= GCC 3.4.0
    // This applies to cin, cout and cerr when not synced with stdio:
    typedef __gnu_cxx::stdio_filebuf<charT, traits> unix_filebuf_t;
    unix_filebuf_t* fbuf = dynamic_cast<unix_filebuf_t*>(stream.rdbuf());
    if (fbuf != NULL) {
        return fbuf->fd();
    }

    // This applies to filestreams:
    typedef std::basic_filebuf<charT, traits> filebuf_t;
    filebuf_t* bbuf = dynamic_cast<filebuf_t*>(stream.rdbuf());
    if (bbuf != NULL) {
        // This subclass is only there for accessing the FILE*.  Ouuwww, sucks!
        struct my_filebuf : public std::basic_filebuf<charT, traits> {
            int fd() { return this->_M_file.fd(); }
        };
        return static_cast<my_filebuf*>(bbuf)->fd();
    }

    // This applies to cin, cout and cerr when synced with stdio:
    typedef __gnu_cxx::stdio_sync_filebuf<charT, traits> sync_filebuf_t;
    sync_filebuf_t* sbuf = dynamic_cast<sync_filebuf_t*>(stream.rdbuf());
    if (sbuf != NULL) {
#  if (__GLIBCXX__<20040906) // GCC < 3.4.2
        // This subclass is only there for accessing the FILE*.
        // See GCC PR#14600 and PR#16411.
        struct my_filebuf : public sync_filebuf_t {
            my_filebuf();  // Dummy ctor keeps the compiler happy.
            // Note: stdio_sync_filebuf has a FILE* as its first (but private)
            // member variable.  However, it is derived from basic_streambuf<>
            // and the FILE* is the first non-inherited member variable.
            FILE* c_file() {
                return *(FILE*)((char*)this + sizeof(std::basic_streambuf<charT, traits>));
            }
        };
        return ::fileno(static_cast<my_filebuf*>(sbuf)->c_file());
#  else
        return ::fileno(sbuf->file());
#  endif
    }
# else  // GCC < 3.4.0 used __GLIBCPP__
#  if (__GLIBCPP__>=20020514)  // GCC >= 3.1.0
    // This applies to cin, cout and cerr:
    typedef __gnu_cxx::stdio_filebuf<charT, traits> unix_filebuf_t;
    unix_filebuf_t* buf = dynamic_cast<unix_filebuf_t*>(stream.rdbuf());
    if (buf != NULL) {
        return buf->fd();
    }

    // This applies to filestreams:
    typedef std::basic_filebuf<charT, traits> filebuf_t;
    filebuf_t* bbuf = dynamic_cast<filebuf_t*>(stream.rdbuf());
    if (bbuf != NULL) {
        // This subclass is only there for accessing the FILE*.  Ouuwww, sucks!
        struct my_filebuf : public std::basic_filebuf<charT, traits> {
            // Note: _M_file is of type __basic_file<char> which has a
            // FILE* as its first (but private) member variable.
            FILE* c_file() { return *(FILE*)(&this->_M_file); }
        };
        FILE* c_file = static_cast<my_filebuf*>(bbuf)->c_file();
        if (c_file != NULL) {  // Could be NULL for failed ifstreams.
            return ::fileno(c_file);
        }
    }
#  else  // GCC 3.0.x
    typedef std::basic_filebuf<charT, traits> filebuf_t;
    filebuf_t* fbuf = dynamic_cast<filebuf_t*>(stream.rdbuf());
    if (fbuf != NULL) {
        struct my_filebuf : public filebuf_t {
            // Note: basic_filebuf<charT, traits> has a __basic_file<charT>* as
            // its first (but private) member variable.  Since it is derived
            // from basic_streambuf<charT, traits> we can guess its offset.
            // __basic_file<charT> in turn has a FILE* as its first (but
            // private) member variable.  Get it by brute force.  Oh, geez!
            FILE* c_file() {
                std::__basic_file<charT>* ptr_M_file = *(std::__basic_file<charT>**)((char*)this + sizeof(std::basic_streambuf<charT, traits>));
#  if _GLIBCPP_BASIC_FILE_INHERITANCE
                // __basic_file<charT> inherits from __basic_file_base<charT>
                return *(FILE*)((char*)ptr_M_file + sizeof(std::__basic_file_base<charT>));
#  else
                // __basic_file<charT> is base class, but with vptr.
                return *(FILE*)((char*)ptr_M_file + sizeof(void*));
#  endif
            }
        };
        return ::fileno(static_cast<my_filebuf*>(fbuf)->c_file());
    }
#  endif
# endif
#else
#  error "Does anybody know how to fetch the bloody file descriptor?"
    return stream.rdbuf()->fd();  // Maybe a good start?
#endif
    errno = EBADF;
    return -1;
}

//! 8-Bit character instantiation: fileno(ios).
template <>
int
fileno<char>(const std::ios& stream)
{
    return fileno_hack(stream);
}

#if !(defined(__GLIBCXX__) || defined(__GLIBCPP__)) || (defined(_GLIBCPP_USE_WCHAR_T) || defined(_GLIBCXX_USE_WCHAR_T))
//! Wide character instantiation: fileno(wios).
template <>
int
fileno<wchar_t>(const std::wios& stream)
{
    return fileno_hack(stream);
}
#endif
```

Phew! So you think this code is grotesque and ugly as sin? Well, it sure is, but at least all the ugly stuff is completely hidden away inside a compiled function and the interface is clear and pristine. If the stream is associated with a file descriptor, then that file descriptor is returned. Otherwise, the function returns -1, signalling an error, since file descriptors are always non-negative. In some cases the implementation falls back to the C library's <code>fileno(3)</code>. Since that sets <code>errno(3)</code> on failure, we might as well set it ourselves if we find that the stream does not correspond to a file descriptor (maybe because it's a <code>stringstream</code>). I don't see too much potential for exceptions. Better keep it in line with the good old POSIX function.

Oh, and yes, you are welcome to drop this function into your own programs. The code is in the public domain.

## What next?

Please email me suggestions and port this hack to your compiler. You can test your success with this little validator. If it compiles and runs without emitting any error message then you're ready to email me the hack so I can include it here. Please email me what preprocessor symbols can be used to distinguish your C++ library implementation from all others. If that library is used by more than one compiler with differing class layouts, it might also become necessary to add preprocessor hacks to distinguish the compilers. If you're convinced that direct access of a stream's file descriptor is needed, then it might be a good idea to request that feature directly from your compiler vendor.

We've had the discussion whether to include a <code>filebuf::fd()</code> member function on GCC's libstdc++ mailing list (one thread started in September, 2001, another one in February, 2004). The issue has resurfaced from time to time with no conclusive results. In my opinion, the implementors of the C++ standard libraries are seriously mistaken when they deny responsibility for this feature. They've argued that file descriptors have no place in the interface the library defines, which is certainly true. However, C++ streams have no place in POSIX either. This vacuum led to the current unpractical situation where certain Unix-specific programming tasks simply cannot be done in C++. Which is unfortunate, in the presence of other, more pragmatic, programming environments.

## History

I'll try to keep this article up to date while recording noteworthy changes in this Changelog. There'll also be links to older versions of the code.

2005-02-13 First public version. Tested on all released versions of GCC 3.x on a variety of platforms. Also works with the Intel compiler, if GCC's libstdc++ is used.
2005-02-18 After ranting on the <libstdc++ at gcc dot gnu dot org> mailing list, I've received a lot of positive feedback. However, it was pointed out that <code>fileno(3)</code> is a macro on some platforms. There, my implementation does not work without some renaming. Minor changes: Jonathan Wakely suggested wrapping the wios-instantiation in #ifdefs and a minor simplification for the GCC-3.4.x filestream case. Also, following a suggestion by Joe Buck, I put the code in the public domain.
Acknowledgments

The animal pictures in this article are taken from Project Gutenberg's EText-No. 11921, The Illustrated London Reading Book. I'ld like to thank Gromit for keeping up my spirit. Lately, Wallace has been quite a nuisance, though. Thanks to Christian Bauer and Bruno Haible for providing valuable criticism, encouragement and suggestions. Also, the Tiergarten Nürnberg and the Biergarten at Atzelsberg have been more than helpful by providing an inspiring backdrop.  ;-)

## Author

Richard B. Kreckel

Last updated 2005-04-02
