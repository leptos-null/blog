# The code we don't write: linking

When you write code in a compiled programming language, how do the functions that we didn't implement run? For example, in a simple C program

```c
#include <stdio.h>

int main(void) {
    puts("Hello, world");
}
```

how does the `puts` function run?

## Implementation location

The implementation (meaning the actual code that will execute) for `puts` must exist somewhere. On macOS, that location is `/usr/lib/system/libsystem_c.dylib`.

### Static linking

Static linking is a type of linking where the linker copies the implementation of the function that your code needs into your binary. In our case, that would mean copying the implementation of `puts` from `/usr/lib/system/libsystem_c.dylib` into our binary.

macOS essentially does not allow statically linking against any system libraries (the reasons for this will be clear later in this post).
On macOS, you can still statically link against code that is not in a system library. In other words, if you were using a third-party library, you may choose to statically link against that library.

Since statically linking copies the implementations your code needs into your binary, you don't need another copy of those implementations on the user's machine.

### Dynamic linking

Dynamic linking is a type of linking where the linker includes information in your binary about the functions that your binary needs.

In our case, that simply means including the information that there's a symbol called `_puts` in the file `/usr/lib/system/libsystem_c.dylib`.

### Linking types

(note: insert diagram from [`linking-diagram-preview.html`](linking-diagram-preview.html))

When in doubt, you should use dynamic linking. Dynamic linking allows multiple programs that use the same library to share that implementation on the filesystem (in static linking, the same implementation would be duplicated across each program). Dynamic linking also allows a library maintainer to deploy bug fixes without requiring each program maintainer to rebuild and deploy their program.

## Terminology

Binary: Binary is frequently used as a generic term in computer science. In this post, I use "binary" to refer to code that can run, either as an executable program, or in a dynamic library. (This is sometimes called a "final linked image".)

Linker: The program responsible for "linking" code and producing a binary. The name of this program is generally `ld`. A compiler driver (e.g. `clang`, `swift`, etc.) usually calls the linker for you.

Symbol: On a technical level, a symbol is a named address within a binary. In C, this is either a function or a global variable. Other languages may choose to annotate other information with symbols; for example, Swift provides symbols for type metadata.

## Platform specific

While the concepts of linking are similar across platforms, this post has already gotten into details that are specific to Apple platforms.

Differences to keep in mind between Apple platforms and Linux:

| component | Apple | Linux |
|    ---    |  ---  |  ---  |
| sysroot | `.sdk` | file system |
| link-time reference | `.tbd` | binaries |
| dynamic library extension | `.dylib` | `.so` ("shared object") |
| symbol convention | prepend `_` (e.g. `_main`) | none (e.g. `main`) |
| object format | Mach-O | ELF |

If you're not familiar with any of those terms, don't worry about them, just keep in mind that not everything in this post is necessarily transferable to Linux or other platforms.

## Inspecting the binary

Let's build our program. Most of the tools I'll be using are pretty stable, but for reference I'm using the tools included with Xcode 26. I've put the code snippet from the beginning of this post into `hello.c`:

```shell
$ clang hello.c -o hello
$ nm hello
0000000100000000 T __mh_execute_header
0000000100000460 T _main
                 U _puts
```

`nm` is showing us the symbols in `hello`. Specifically, we're seeing `{hexadecimal address} {type} {name}`.

Per [`nm` docs](<https://llvm.org/docs/CommandGuide/llvm-nm.html>):

| code | description |
| --- | --- |
| T | Code (text) object |
| U | Named object is undefined in this file |

(table truncated)

"Undefined" means that the implementation is not included in the provided file.

Reading out each line of the `nm` output:

`__mh_execute_header` is a symbol added by the linker - it's the `mach_header` object - we don't need to worry about this.

`_main` is the `main` function that we wrote in C - the code (which means the assembled instructions, not the source code) is in the binary.

`_puts` is the `puts` function - we were expecting the code for it to be in `/usr/lib/system/libsystem_c.dylib`, which is not our binary, so `nm` reporting the symbol as "undefined" makes sense.

We can open `hello` in `lldb`:

```lldb
(lldb) target create hello
Current executable set to 'hello' (arm64).
(lldb) target modules lookup -n puts
1 match found in /usr/lib/system/libsystem_c.dylib:
        Address: libsystem_c.dylib[0x00000001803abe9c] (libsystem_c.dylib.__TEXT.__text + 176100)
        Summary: libsystem_c.dylib`puts
```

`lldb` found `puts` in `/usr/lib/system/libsystem_c.dylib`, confirming our expectation.

Lastly, we can run the binary and see that it works as expected:

```shell
$ ./hello
Hello, world
```

## Finding the symbol at link-time

We just saw with `lldb` that `puts` is in `/usr/lib/system/libsystem_c.dylib`. `lldb` is telling us this information at run-time. How does the linker (which runs at link-time) know this information?

If we check [`man ld`](<https://man.freebsd.org/cgi/man.cgi?query=ld&sektion=1&manpath=macOS+26.6.1>):

> **Search paths**
>
> **ld** maintains a list of directories to search for a library or framework to use. The default library search path is `/usr/lib` then `/usr/local/lib`. The `-L` option will add a new library search path. [...] The `-syslibroot` option will prepend a prefix to all search paths.
>
> **-l**x
>
> This option tells the linker to search for libx.dylib or libx.a in the library search path. If string x is of the form y.o, then that file is searched for in the same places, but without prepending `lib` or appending `.a` or `.dylib` to the filename.

(note: in HTML, try making this code-block look more like a `man` page)

In short, the linker needs to be told (via command line options):

- the names of the libraries that will be needed to link against
- the folders where libraries may be

As we know from the [Terminology section](#Terminology), a compiler driver usually calls the linker. Here's how some compiler drivers work:

- Usually, a C compiler must be told by the developer (via command line options) which libraries they would like to link against. The example we saw above with `clang` worked because the compiler driver automatically links against the C standard library, which is where `puts` is.
- Clang added a feature called [modules](<https://clang.llvm.org/docs/Modules.html>) which you can opt into with the `-fmodules` command line option. These modules aim to address multiple behaviors with C. One of those behaviors is to automatically link a library when one of its headers is used (I'll demonstrate this below).
- Swift uses essentially the same module system as Clang.

To demonstrate the Clang behavior, we'll use the following C program, in a file called `zlib_version.c`:

```c
#include <stdio.h>
#include <zlib.h>

int main(void) {
    printf("zlibVersion: %s\n", zlibVersion());
}
```

When I build this:

```shell
$ clang zlib_version.c -o zlib_version
Undefined symbols for architecture arm64:
  "_zlibVersion", referenced from:
      _main in zlib_version.o
ld: symbol(s) not found for architecture arm64
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```

The symbol wasn't found. We can infer from the information above that the library wasn't passed to `ld` via the `-l` option. Clang lets us tell it the libraries we want to link against using the same spelling as `ld`, so we can pass that through:

```shell
$ clang -lz zlib_version.c -o zlib_version
$ ./zlib_version
zlibVersion: 1.2.12
```

Now it works. Let's try with `-fmodules` as well:

```shell
$ clang -fmodules zlib_version.c -o zlib_version
$ ./zlib_version
zlibVersion: 1.2.12
```

That also works.

## Finding the symbol at run-time

When you run any program on Apple platforms, the first code that runs in the process is `dyld`, specifically, the [`start` function](<https://github.com/apple-oss-distributions/dyld/blob/fd8d0c4d52320ebf64db34f3cb280310d905c5ae/dyld/dyldMain.cpp#L1340>).

<!-- TODO -->
