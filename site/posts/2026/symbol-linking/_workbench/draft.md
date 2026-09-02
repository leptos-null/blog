# The code we don't write: linking

When you write code in a compiled programming language, how do the functions that we didn't implement run? For example, in a simple C program:

```c
#include <stdio.h>

int main(void) {
    puts("Hello, world");
}
```

How does the `puts` function run?

## Terminology

Binary: Throughout computer science, binary is frequently used as a generic term. In this post, I use "binary" to refer to code that can run, either as an executable program or in a dynamic library. (This is sometimes called a "final linked image".)

Linker: The program responsible for "linking" code and producing a binary. The name of this program is generally `ld`. A compiler driver (e.g. `clang`, `swift`, etc.) usually calls the linker for you.

Symbol: On a technical level, a symbol is a named address within a binary. In C, this is either a function or a global variable. Other languages may choose to annotate other information with symbols; for example, Swift provides symbols for type metadata.

## Two linking types

The implementation (meaning the actual code that will execute) for `puts` must exist somewhere. On macOS, that location is `/usr/lib/libSystem.B.dylib` (actually, it's `/usr/lib/system/libsystem_c.dylib` because `libSystem.B.dylib` is what's called an "umbrella" that re-exports symbols from other libraries, but that's outside the scope of this post).

### Static linking

Static linking is a type of linking where the linker copies the implementation of the function that your code needs into your binary. In our case, that would mean copying the implementation of `puts` into our binary. Static linking requires the implementation to be in a static archive (`.a`) file.

Since static linking copies the implementations your code needs into your binary, you don't need a separate file with those implementations on the user's machine. `/usr/lib/libSystem.B.dylib` is already on the user's machine (since it ships with the operating system), so this wouldn't be helpful. If you were using a third-party library, you may choose to statically link against that library.

### Dynamic linking

Dynamic linking is a type of linking where the linker includes information in your binary about the functions that your binary needs so they can be "dynamically" found when your binary runs. In our case, that simply means including the information that there's a symbol called `_puts` in the file `/usr/lib/libSystem.B.dylib`.

Dynamic linking requires that the libraries you link against at link-time are also present on the user's machine when your binary runs. This is a common use for package managers such as `apt` or `brew`: your program or library can depend on libraries provided by other packages. You communicate the dependencies your package has and the package manager ensures those dependencies are present on the user's machine so your binary can run.

### Comparison

(note: insert diagram from [`linking-diagram-preview.html`](linking-diagram-preview.html))

When in doubt, you should use dynamic linking. Dynamic linking allows multiple programs that use the same library to share that implementation on the filesystem (in static linking, the same implementation would be duplicated across each program). Dynamic linking also allows a library maintainer to deploy bug fixes without requiring each program maintainer to rebuild and deploy their program.

The remainder of this post focuses on dynamic linking.

## Platform specifics

While the concepts of linking are similar across platforms, this post gets into details that are specific to Apple platforms.

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

For this blog post, I asked Claude to write a program to list information about the symbols in a given binary.
I'll be referring to this program as `./symlist`. You can view the source code for this program on [GitHub](<https://github.com/leptos-null/blog/blob/main/site/posts/2026/symbol-linking/_workbench/symlist.c>) or [here on the blog](./symlist.c).

(note: in HTML, the second link should be to `symlist.html` which is a code-block with lsp markup for `symlist.c`)

`nm` is a similar tool that ships with the Xcode toolchain, if you'd like to follow along without building `./symlist`.

I've put the code snippet from the beginning of this post into `hello.c`:

```shell
$ clang hello.c -o hello
$ nm -m hello
0000000100000000 (__TEXT,__text) [referenced dynamically] external __mh_execute_header
0000000100000460 (__TEXT,__text) external _main
                 (undefined) external _puts (from libSystem)
$ ./symlist hello
_main: 0x0000000100000460 [extern]
_puts: /usr/lib/libSystem.B.dylib [extern]
```

The main reason I wanted to use `./symlist` for this post instead of `nm` is to show that the full path information is included in the `hello` binary (`nm` most likely trims the path for readability).

The takeaway from this section is that we're able to view the information that the linker placed in the binary about the symbol that I previously mentioned in the [Dynamic linking](#dynamic-linking) section.

## Finding the symbol at link-time

We now know (from the previous section) that the binary contains the full path of the library for each symbol it dynamically links against. How does the linker know that information to be able to put it there?

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

- the names of the libraries to link against
- the folders where libraries may be located

As we know from the [Terminology section](#terminology), a compiler driver usually calls the linker. Here's how some compiler drivers work:

- Usually, a C compiler must be told by the developer (via command line options) which libraries they would like to link against. The example we saw above with `clang` worked because the compiler driver automatically links against the C standard library, which is where `puts` is.
- Clang added a feature called [modules](<https://clang.llvm.org/docs/Modules.html>) which you can opt into with the `-fmodules` command line option. These modules aim to improve multiple patterns when using C. One of these improvements is to automatically link the required library when your code includes a header defined to be in the module (I'll demonstrate this below).
- Swift uses a module system similar to Clang.

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

The linker didn't find the symbol. We can infer from the information above that the library wasn't passed to `ld` via the `-l` option. We can ask `clang` to pass that information explicitly:

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

That also works. This post isn't meant to be too much about modules- if you want, you can find the module map file for the relevant module here with `grep -rE '\<header\>\s+"zlib\.h"' "$(xcrun --show-sdk-path)/usr/include" --include='*.modulemap'`.

We can inspect that binary again:

```shell
$ nm -m zlib_version
0000000100000000 (__TEXT,__text) [referenced dynamically] external __mh_execute_header
0000000100000498 (__TEXT,__text) external _main
                 (undefined) external _printf (from libSystem)
                 (undefined) external _zlibVersion (from libz)
$ ./symlist zlib_version
_main: 0x0000000100000498 [extern]
_printf: /usr/lib/libSystem.B.dylib [extern]
_zlibVersion: /usr/lib/libz.1.dylib [extern]
```

### Text Based Dynamic Library Stub

To check that `/usr/lib/libz.1.dylib` contains a symbol named `_zlibVersion`, the linker would have to open `/usr/lib/libz.1.dylib` and parse the symbol table. `/usr/lib/libz.1.dylib` has a lot more information than just the symbol table (on macOS 26.5.2, just the arm64e slice is about 120 kB, which I determined using `target modules dump sections /usr/lib/libz.1.dylib` in LLDB).

Instead of distributing the entire `.dylib` (dynamic library), Apple uses `.tbd` (text based dylib) files for the linker to use. Here's a look at `/usr/lib/libz.tbd` (within the SDK):

```tbd
--- !tapi-tbd
tbd-version:     4
targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
install-name:    '/usr/lib/libz.1.dylib'
current-version: 1.2.12
exports:
  - targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
    symbols:         [ _adler32, _adler32_combine, _adler32_z, _compress, _compress2, 
                       _zlibVersion ]
...
```

I've truncated this snippet for this post, but the full file is only 1.8 kB (macOS 26.5 SDK). Not only does a `.tbd` not contain any of the actual code that a `.dylib` would, but as you can see in this snippet, symbols that appear in multiple architecture slices can be combined into a single listing (in the vast majority of cases, the list of symbols in each slice of a library are the same).

`ld` still supports using `.dylib` files directly if it's given one (which is common when building your own project if you split your code into dynamic libraries).

You can produce your own `.tbd` files using the `tapi` tool that's included in the Xcode toolchain.

## Resolving the symbol at run-time

On Apple platforms, when a program starts, the first code that runs is in a binary called `dyld` ("the dynamic linker"). `dyld` performs a lot of start-up work, but the part that's important to us is that it "links" the symbols we've been talking about.

Here's approximately what that would look like for the `hello` program:

1. `dyld` sees that the program requires a symbol called `_puts` which should be in `/usr/lib/libSystem.B.dylib`
2. `dyld` loads `/usr/lib/libSystem.B.dylib` into memory
3. `dyld` finds the address of `_puts` in memory
4. `dyld` places this address into a pointer that `hello` uses when calling `puts`

## Poking holes

What if we produce a binary that references a file that doesn't exist at run-time? Or a symbol that doesn't exist within a file that does?

We'll begin by creating a copy of the SDK: `ditto "$(xcrun --show-sdk-path)" MacOSX.sdk` (this takes a few seconds to run - the SDK is a lot of files).

The SDK I'm using for this is `macosx26.5`. Most likely you can follow along with any similar SDK.

Let's revisit the [Finding the symbol at link-time](#finding-the-symbol-at-link-time) section: one of the commands we used to build `zlib_version` was `clang -lz zlib_version.c -o zlib_version`. There's no `-L`, so the library search path is just the default: `/usr/lib` then `/usr/local/lib`. Lastly, remember that `-lx` "tells the linker to search for libx.dylib or libx.a in the library search path". That documentation leaves out `.tbd`, but that's included too.

If we pretend like we're the linker for a moment, we should first check `/usr/lib/libz.{dylib,tbd,a}` (also remember that the sysroot is prepended to these paths): we've got a match at `MacOSX.sdk/usr/lib/libz.tbd`. We can check that the symbol we're searching for is present and we're done.

Now let's mess with this file a little bit.

Let's start by switching out the path:

```diff
@@ -1,8 +1,8 @@
 --- !tapi-tbd
 tbd-version:     4
 targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
-install-name:    '/usr/lib/libz.1.dylib'
+install-name:    '/usr/lib/libzebra.dylib'
 current-version: 1.2.12
 exports:
   - targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
     symbols:         [ _adler32, _adler32_combine, _adler32_z, _compress, _compress2, 
```

(If you wanted to do this with a `.dylib` instead, this is just as possible using `install_name_tool` which is included with the Xcode toolchain.)

Compile the same `zlib_version.c` file from earlier, but now against our edited SDK:

```shell
$ clang -isysroot MacOSX.sdk -lz zlib_version.c -o zlib_version
$ ./zlib_version
dyld[88918]: Library not loaded: /usr/lib/libzebra.dylib
  Referenced from: <E7847535-85C4-3B14-8DDF-4532B4F82720> ./zlib_version
  Reason: tried: '/usr/lib/libzebra.dylib' (no such file), '/System/Volumes/Preboot/Cryptexes/OS/usr/lib/libzebra.dylib' (no such file), '/usr/lib/libzebra.dylib' (no such file, not in dyld cache)
[1]    88918 abort      ./zlib_version
```

The process crashed immediately, with a helpful error message. Apple publishes the source code for `dyld`, so we can even see the source of this message: <https://github.com/apple-oss-distributions/dyld/blob/fd8d0c4d52320ebf64db34f3cb280310d905c5ae/dyld/JustInTimeLoader.cpp#L525>.

Let's put the path back and edit one of the symbol names:

```diff
@@ -3,9 +3,9 @@
 targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
 install-name:    '/usr/lib/libz.1.dylib'
 current-version: 1.2.12
 exports:
   - targets:         [ x86_64-macos, x86_64-maccatalyst, arm64e-macos, arm64e-maccatalyst ]
-    symbols:         [ _adler32, _adler32_combine, _adler32_z, _compress, _compress2, 
+    symbols:         [ _adler32, _adler32_combine, _adler32_z, _zebra_compress, _compress2, 
                        _compressBound, _crc32, _crc32_combine, _crc32_combine_gen, 
```

(If we had to use a `.dylib` here, we could trick the linker by compiling our own `.dylib` where the source code is just empty functions for each desired symbol.)

We'll need a new source code file to test this. I'll create `fake_zebra.c`:

```c
extern int zebra_compress(void);

int main(void) {
    zebra_compress();
    return 0;
}
```

```shell
$ clang -isysroot MacOSX.sdk -lz fake_zebra.c -o fake_zebra
$ ./fake_zebra
dyld[89724]: Symbol not found: _zebra_compress
  Referenced from: <1620228C-A5B2-3A58-9BF6-C87F3549808C> ./fake_zebra
  Expected in:     <863BB312-F115-32B0-889F-D3B42857A7A6> /usr/lib/libz.1.dylib
[1]    89724 abort      ./fake_zebra
```

A fairly similar and helpful message from `dyld`.

In both of these cases, we made deliberate changes to the `.tbd` that we reasonably expected would cause problems. You may see these errors with less deliberate changes though. For example, if you make changes to the source code of a library you maintain, you may update the headers and `.tbd`, but forget to install the library itself. If you had the old version of your library still installed and you ran a program that linked against the new `.tbd` using new symbols, you might see the "Symbol not found" error. If you had uninstalled the old version of your library first, you would likely see the "Library not loaded" error.

## Conclusion

Back to the question we started with: "How does the `puts` function run?"

The implementation for the `puts` function exists in a library on the user's computer. The linker (`ld`) includes the path of that library in your binary. When your binary runs on the user's machine, the dynamic linker (`dyld`) loads the library from the path and gives the memory address of `puts` to your binary. The code in your binary is then able to jump to that address just like it would when calling any other function.
