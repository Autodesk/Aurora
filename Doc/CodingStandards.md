> Always code as if the guy who ends up maintaining your code will be a violent psychopath who knows where you live.
>
> — John F. Woods

Please observe the following coding standards when contributing to Aurora.

## Table of Contents

- [Rule Zero: No Warnings or Memory Leaks](#rule-zero-no-warnings-or-memory-leaks)
- [Rule One: Implementation Comments](#rule-one-implementation-comments)
- [Doc Comments](#doc-comments)
- [Linting and Formatting Tools](#linting-and-formatting-tools)
  - [clang-tidy](#clang-tidy)
  - [clang-format](#clang-format)
- [Secure Coding Guidelines](#secure-coding-guidelines)
- [C++ Features](#c-features)
  - [Overview](#overview)
  - [Arrays](#arrays)
  - [Auto](#auto)
  - [Callback Functions](#callback-functions)
  - [Casting](#casting)
  - [Conditional Statements](#conditional-statements)
  - [Enumerations](#enumerations)
  - [Files and Paths](#files-and-paths)
  - [Integer Types](#integer-types)
  - [Loops](#loops)
  - [Macros](#macros)
  - [Member Initialization](#member-initialization)
  - [Namespaces](#namespaces)
  - [Numbers](#numbers)
  - [Override Keyword](#override-keyword)
  - [Smart Pointers](#smart-pointers)
  - [Strings and Unicode](#strings-and-unicode)
  - [Type Aliases](#type-aliases)
- [Formatting](#formatting)
  - [Accessors](#accessors)
  - [Complexity](#complexity)
  - [File and Directory Names](#file-and-directory-names)
  - [Identifiers](#identifiers)
  - [Indenting](#indenting)
  - [Line Length](#line-length)
  - [Spacing Inside Expressions](#spacing-inside-expressions)
  - [Whitespace](#whitespace)
- [Header Files](#header-files)
  - [Including Header Files](#including-header-files)
  - [Including Header Files - Example](#including-header-files---example)
  - [Member Order](#member-order)
- [Logging](#logging)
- [Error Handling](#error-handling)
- [CMake](#cmake)
- [Binaries](#binaries)

## Rule Zero: No Warnings or Memory Leaks

* **NO linter / compiler warnings or memory leaks.**

Why zero? It is easy to spot one new warning or memory leak when you don't have any to begin with, but difficult when you already have many of them.

**Warnings:** You can use warning or error overrides (e.g. `pragma warning disable`) if the underlying issue can't be fixed, but such overrides _must_ be commented. This may be unavoidable for third-party libraries. Overrides should be added in the code at the specific points where they are needed, and not in the CMake scripts. Note that Aurora is built with a high warning level (e.g. [`/W4` in Visual C++](https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=vs-2019)), and with warnings treated as errors.

**Memory Leaks:** You must not introduce obvious memory leaks, i.e. from launching and exiting a test application with simple data. Memory leaks are usually reported on application shutdown, and this report should always be empty with simple testing.

- Windows: CPU memory leaks will be reported as `Detected memory leaks!` followed by the relevant memory locations.
- Direct3D 12: GPU memory leaks will be reported with `D3D12 Warning` or `DXGI Warning` followed by the relevant memory locations.

## Rule One: Implementation Comments

> The purpose of commenting is to help the reader know as much as the writer did.
>
> — "The Art of Readable Code" (2012)

**All code must include programmer-level documentation as comments.** Comments are absolutely essential for proper understanding, debugging, and modification of existing code. Other developers looking at your code (and yourself after you have forgotten the code!) are depending on this.

Comments should convey **design intent**, i.e. more about "why" and less about "how." This can be supplemented with design documentation, but comments should be the first source for developers working on the code. Recovering design intent without comments or documentation is very difficult, and often involves laborious "code archaeology." Specifically, It should not be necessary to consult the original author (who may no longer be available) or comb through old commits to understand how code works.

Some specific guidelines include:

* Implementation comments (usually inside functions) are extremely important.
  * Each non-trivial "block" of code should have a comment.
  * A block is therefore: empty line, comment, lines of code, followed by an empty line.
  * Avoid end-of-line comments and comments that are not at the top of blocks.
* On average, there will be a comment for roughly every ten lines of code. In some cases, the number of lines of comments will exceed the number of lines of code!
* Add a `// TODO`: comment starting on its own line to highlight and describe missing, incomplete, or potentially error-prone code. You may refer to the ID of a corresponding GitHub issue, e.g. `// TODO: <Some explanation.> See issue #42 for details.`
* Add a `// NOTE`: comment starting on its own line for background information, e.g. additional details on why something is being done, or links to additional information.
* You may include links to public references (e.g. technical papers) that don't require authentication. However, never include links to _internal_ issue tracking systems, wikis, etc.
* Remove all wizard-generated comments unless they are actually useful.
* Avoid comments that explain the obvious.
* All comments should be kept up-to-date. Make sure to add new comments or edit existing comments when editing existing code.
* All comments follow these guidelines for a professional appearance and readability:
  * Use complete sentences, i.e. start with a capital letter and end with a period.
  * Sentences should be separated by single spaces (not double).
  * Spelling should be correct.
  * Avoid the use of abbreviations like "dtor."
  * Acronyms can be used depending on the context of the code, e.g. "UAV" (unordered access view) in the context of low-level rendering code, but not in high-level API where the acronym may not be understood.
* Do not comment out obsolete code; simply delete it. It can be retrieved with source control history if needed.
* If you want to retain a small amount of commented out reference code, include a comment explaining why it is commented out. For example, "Uncomment this code to log additional information."

## Doc Comments

- Include doc comments for classes and their public members in their corresponding header files.

- Doc comments are specially formatted comments used to generate documentation for types and functions. We use [Doxygen](http://www.doxygen.nl) to generate this documentation, which is used by clients.

- This is different from the implementation comments described in "Rule One" above. Both are important: implementation comments for internal developers, and doc comments for external (and internal) developers.

- At a minimum, **every public type, enumerator, and function** (including public members) must have a brief, single sentence doc comment. Beyond this, it is desirable to include comments for function parameters and return values, especially when they are not trivial.

- We use the following doc comment style:

  ```c++
  /// A brief description, and _only_ a single sentence, ending in a period.
  ///
  /// An empty line above, followed by more information. Doxygen will automatically separate the
  /// single sentence (above) and this longer description. This can be multiple sentences and
  /// wrap multiple lines like this. Follow this with another empty line.
  ///
  /// \note Optional note which appears separate in the generated documentation. This can be used
  /// for special guidance or disclaimers to developers.
  /// \param myParameterName Describe the parameter here. The first word after "\param" must be
  /// the parameter name. This can be multiple sentences and wrap multiple lines like this. It is
  /// not necessary to mention default arguments; they appears in the generated documentation.
  /// \return Describe the return value here.
  bool doSomething(float myParameterName = 1.0f);
  ```

- Use the present tense (usually ending in "s") and articles ("the" "a" "an") for function descriptions, e.g. "Compute**s** the length of the vector."

## Linting and Formatting Tools

If you encounter unexpected issues with automatic linting and formatting tools, please log an issue in GitHub or propose an appropriate override. As with disabling warnings, any overrides must be commented.

### clang-format

We use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) to automatically format code to be compliant with the standards.
- Formatting rules are defined by the [.clang-format](/.clang-format) file in the repository root folder.
- clang-format operates on several file formats including (but not limited to): C, C++, C#, CUDA, JavaScript, JSON, and Objective-C. It does not currently operate on shader code (e.g. HLSL or Slang), so these must be formatted manually.
- Any lines of source code that should not be formatted by clang-format should be surrounded by `// clang-format off` and `// clang-format on` single-line comments.
- To manually run clang-format on a file in Visual Studio, use _Edit | Advanced | Format Document_ (Ctrl-K+D).
- It is recommended to use the clang-format [pre-commit](https://pre-commit.com) hook, that is setup via the [YAML config file](/.pre-commit-config.yaml). Enable this with the following steps:
  1. Open a command prompt in the source root folder.
  2. Ensure the Python executable and `Scripts` folder is in your system path.
  3. Install pre-commit with the command: `python -m pip install pre-commit`
  4. Install the hooks with command: `pre-commit install`
  5. All subsequent Git commits will invoke clang-format on the changed files. If any changes are made by this process, it means that one or more files have incorrect formatting and the commit will fail. The developer should ensure the newly modified files are added to the commit and attempt to commit again. 
  6. If needed clang-format can be invoked on all the files in the repository with the command: `pre-commit run --all-files`

## C++ Features

### Overview

- **We generally follow the [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).**
- Any features from **C++17 or earlier** are assumed to be available.
- If you find it necessary to use features in a new version of C++, please log a request as a GitHub issue.
- Avoid use of "clever" techniques that use new features, but that don't improve readability, performance, or stability.
  - For more perspective on possible pitfalls of "clever" use of C++, see [this blog post](https://aras-p.info/blog/2018/12/28/Modern-C-Lamentations).
  - In other words: you should not need a "PhD in C++" to develop for Aurora.
- The STL types (or near-identical implementations) are now part of the [C++ Standard Libary](https://en.wikipedia.org/wiki/C%2B%2B_Standard_Library), so they can appear in public header files.

### Arrays

- For fixed-size arrays, prefer the use of `std::array<>`.
- For variable-size arrays, prefer the use of `std::vector<>`.
- As a convenience, unique fixed-size arrays can be created with `std::make_unique<YourType[]>(yourSize)`.
  - Support for shared fixed-size arrays with `std::make_shared` is only available with C++20.
  - An alternative is to use `std::make_shared<vector<YourType>>(yourSize)`.
- If possible, use `reserve` to pre-allocate vectors with their known sizes.

### Auto

- [The `auto` keyword](https://en.cppreference.com/w/cpp/language/auto) should be used to automatically deduce types when the type is otherwise obvious or would be cumbersome to include.
- Conversely, do not use `auto` when it prevents a developer from knowing the type by simply reading the code without tools, e.g. in GitHub. An exception is iterators: `auto` can and should be used for iterator types.
- Use an asterisk for pointer types: `auto* pMyPointer...` The keyword works the same with and without the asterisk; we are using it for clarity.
- Specifically, follow the guidelines of the clang-tidy [modernize-use-auto](https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-auto.html) check.

### Callback Functions

- Use `std::function<>` as the type for function parameters that accept callback functions.
- This allows the caller to specify the callback function in one of several ways.
- [See this code sample for more details](https://github.com/pixnblox/Tutorials/tree/master/Callback%20Functions).

### Casting

- Use C++ casts instead of C-style casts, e.g. `static_cast<int>(myFloat)` instead of `(int)myFloat`.
- These include: `static_cast<> const_cast<> dynamic_cast<> reinterpret_cast<>`.
- For more information, see this [Stack Overflow question and answer](https://stackoverflow.com/questions/332030/when-should-static-cast-dynamic-cast-const-cast-and-reinterpret-cast-be-used).

### Conditional Statements

- It is _not_ necessary to compare against `nullptr` when testing pointers for values, e.g. `if (pFoo != nullptr)`. This is safe with raw pointers in modern C++, and directly supported with a boolean operator for smart pointers. Also, the guideline of using a "p" prefix makes it clear to the reader what is happening.
- For an an `if / else` statement, prefer using the positive state in the `if` statement, so it appears first. For example, use `if (myFlag) / else ...` instead of `if (!myFlag) / else ...`.
- A boolean **false and true** automatically and safely converts to an integer **zero and one** (respectively). Similarly, an integer zero converts to false, and any other integer converts to true. Casting is not necessary. For example, it is valid to use a bitwise operator to get a test result like `bool result = testBits & value` . Similarly, it is not necessary to cast `int value = functionThatReturnsBool()`.

### Enumerations

- Enumerations should be declared with `enum class` instead of simply `enum`.
- Enumerators of such enumerations have their own scope, and are not implicitly converted to integers.
- Enumerators should be prefixed with "k" like constants: `kMyEnumerator`.
- [See the C++ Core Guidelines for details](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#enum3-prefer-class-enums-over-plain-enums).

### Files and Paths

- Do not parse or create file names or paths manually with string manipulation, as this can be error-prone across operating systems.
- Instead use (and preferably encapsulate) operating system functions. For example, on Windows the `Path*` functions from [shlwapi.h](https://docs.microsoft.com/en-us/windows/win32/api/shlwapi) can be used safely.
- Note that we can't use the [STL filesystem library](https://en.cppreference.com/w/cpp/filesystem) yet due to lack of support on macOS before 10.15 (Catalina).
- For identifiers referring to file paths, prefer the use of the word "path" unless a file _name_ is specifically being referenced, i.e. not including possible directories. For example, `setFilePath` instead of `setFileName`. Also note that "file name" or "file path" is always two words.

### Integer Types

- Use `int` in most cases, where numbers are not particularly large (or very negative).
- Use the [fixed width integer types](https://en.cppreference.com/w/cpp/types/integer) from `<cstdint>`, such as `int64_t` when you need a guaranteed size.
- Use `size_t` for variables referring to sizes in memory, including offsets and strides.
- Avoid the used of unsigned integer types.
  - In particular, don't use them to indicate than a value can't be negative; use an assert instead.
  - There are cases where they are needed, e.g. bit manipulation or byte arrays.
  - [See here for more information](https://www.learncpp.com/cpp-tutorial/unsigned-integers-and-why-to-avoid-them/).
- Use `uint8_t` for byte arrays, at least until we can use `std::byte` from C++17.

### Loops

- Use [range-based loops](https://en.cppreference.com/w/cpp/language/range-for) when possible, instead of traditional `for` loops.
- This can be done with statically allocated arrays, containers (using iterators), and array-like containers (with the `[]` operator).
- Specifically, follow the guidelines of the clang-tidy [modernize-loop-convert](https://clang.llvm.org/extra/clang-tidy/checks/modernize/loop-convert.html) check.

### Macros

- Macros are function-like preprocessor definitions, defined with `#define`. These have a number of properties that make them [dangerous to use](https://isocpp.org/wiki/faq/inline-functions#inline-vs-macros), so avoid creating them.
- Inline functions and template function can often be used to accomplish the same tasks more reliably.
- If you must define macros, use UPPER_CASE for names: `MY_MACRO`

### Member Initialization

- Use [default member initializers](https://en.cppreference.com/w/cpp/language/data_members) instead of a default constructor's member initializers.

- This capability was expanded in C++11; it was previously limited to static const integral types.

- For example:

  ```c++
  public:
      MyClass::MyClass() : _myMember(1.0f) {} // don't do this...
      float _myMember;
      
  public:
      MyClass::MyClass() {}
      float _myMember = 1.0f; // ... instead do this
  ```

- [See this blog post for examples](https://abseil.io/tips/61).

### Namespaces

- When declaring a namespace, don't indent the contained code; there is no real advantage to indenting and it just loses some (useful) space.
- At the end of a namespace declaration, use a comment at the closing brace to make it clear what is ending, e.g. `} // namespace Aurora`.
- When using global identifiers, such as from the Win32 API, explicitly refer to the global namespace with the `::` operator to make this clear, e.g. `::SetWindowText(...)`.
- Be careful not to pollute the client's namespace with `using` directives.
- You may apply `using` to use common namespaces like `std` in internal code that is not accessed by clients. This is typically done in the precompiled header file (`pch.h`). In this case, the relevant namespace should be omitted from identifiers, e.g. use simply `shared_ptr` instead of `std::shared_ptr`.

### Numbers

- Avoid the use of "magic" numbers that don't have an explicit meaning.
- Use named constants instead. For example, instead of using the constant "4" to refer to the number of samples for a function call (`myFunction(4);`), define a constant and use it: `int kSampleCount = 4; myFunction(kSampleCount);`.
- Constants with a complex or non-intuitive meaning should have a corresponding comment.

### Override Keyword

- Use the `override` keyword on functions overridden in derived classes...
- ... and do not use `virtual` on overridden functions (only in the base declaration) .
- Specifically, follow the guidelines of the clang-tidy [modernize-use-override](https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-override.html) check.

### Smart Pointers

- Use smart pointers to control object lifetime:

  - Use the `std::shared_ptr<>` type for sharing objects with a reference count.
  - Use the `std::unique_ptr<>` type for retaining (or transferring, with [std::move](https://en.cppreference.com/w/cpp/utility/move)) exclusive control of the object.
  - Use the `std::weak_ptr<>` type to break potential circular dependencies between smart pointers, if needed.

- Create objects with `std::make_shared<>` and `std::make_unique<>`, instead of `new` / `delete`.

- Exception: Use `ComPtr` for lifetime management of DirectX objects, and any other COM objects.

  - Use `<wrl/client.h>` to include this class.
  - Since this type should not appear in public API, you can declare `using Microsoft::WRL::ComPtr;`.
  - See [this DirectXTK wiki page](https://github.com/Microsoft/DirectXTK/wiki/ComPtr) for more information.

- Pass / return objects and smart pointers in functions as follows:

  - Pass smart pointers by const reference (not by value) when the function needs to share (or transfer) ownership.
  - Pass smart pointers by reference only when the the smart pointer itself must be manipulated; this is not common.
  - In other cases (which are the most common), prefer passing objects by reference (when not nullable) or raw pointer (when nullable), not by smart pointer.

  - Return smart pointers by value (never by reference) when the function needs to share (or transfer ownership). Otherwise return the raw pointer.
  - [Much more on this subject can be found here](https://herbsutter.com/2013/06/05/gotw-91-solution-smart-pointer-parameters). Note that it suggests passing by value instead of const reference, but the argument for this is not very strong, and it would be inconsistent with common practice for other types.
- Because smart pointers initialize to `nullptr` and it is not necessary to compare to `nullptr` (see [Conditional Statements](#conditional-statements)), the direct use of the `nullptr` keyword should be rare. Raw pointers, where used, should still be initialized to `nullptr`.
- It is common to alias smart pointers with a type ending in "Ptr" such as `IRendererPtr` for `shared_ptr<IRenderer>`. This allows for brevity as well as hinting that a smart pointer is being used. Do not use this suffix to alias raw pointer types.

### Strings and Unicode

- Use `std::string` with UTF-8 encoding as much as possible. UTF-8 can represent all possible [Unicode code points](https://en.wikipedia.org/wiki/List_of_Unicode_characters).
- Do not assume that each byte is one character when using UTF-8 encoding, i.e. `string::length()` will not necessarily return the string length in characters.
- On Windows:
  - You MUST use the "W" versions of Win32 functions and wide character overloads of Standard Library functions like the `ifstream` constructor.
  - **This is the only way to support Unicode on Windows.** The "A" functions and Standard Library narrow character overloads don't support UTF-8 and should not be used.
  - "Widen" the UTF-8 strings to UTF-16 as needed (and as close as possible to the calls) to provide inputs to / get results from the wide functions. Use the functions `Aurora::Foundation::s2w()` and `Aurora::Foundation::w2s()` or [the code in this gist](https://gist.github.com/pixnblox/b4980243334a1292bed957493941f071) to convert to and from UTF-16 strings.
  - String literals with special characters (which are rare) must be declared with wide characters, and then narrowed for storage.
  - Third-party libraries that accept narrow character inputs require special support for Windows, as described above. Verify such support is present before using them.
- For more on this: [http://utf8everywhere.org](http://utf8everywhere.org/).

### Type Aliases

Do not use `typedef` for type aliases. Instead use the `using` keyword to create an [alias declaration](https://en.cppreference.com/w/cpp/language/type_alias).
- `typedef int MyType;` ⬅ do not do this
- `using MyType = int;` ⬅ do this instead

## Formatting

NOTE: clang-format should enforce most of these standards, but still try to observe them as you write.

### Accessors

- For set accessors, use a `set` prefix, e.g. `void setViewMatrix(mat4 value)`.
- For trivial get accessors, or those that perform lazy evaluation, _do not_ use a prefix, e.g. `const mat4& viewMatrix() const`. This is an indication to the caller that it is OK to call the function many times in a row if needed.
- For trivial get accessors that use reference parameters for return values, use a `get` prefix, e.g. `void getDimensions(int& width, int& height) const`.
- For get accessors that perform non-trivial work, use the prefix `compute` instead, e.g. `float computeDistance() const`. This is an indication to the caller that the function should not be called repeatedly.
- For all get accessors, where possible declare functions as const and return values as const references, as in the `viewMatrix` example above.

### Braces

- Braces should appear on their own lines.
- Single-line statements inside conditional expressions should be surrounded by braces.
```
if (foo == bar)
{
    return foo;
}
else
{
    bar++;
}
```

### Complexity

- Avoid long functions (e.g. over 50 lines of code, not including comments), and refactor them into separate functions as needed.
- Similarly, avoid long indented blocks of code, e.g. the body of a conditional statement.
- When closing a `#if` preprocessor directive after several lines of code, use an end-of-line comment that matches the definition name, similar to what is expected for namespace declarations, e.g.
```
#if defined(_WIN32)
   // Several
   // lines
   // of
   // code
#end // _WIN32 <- add this
```
- On a related note, use the `#if` directive with `defined()` instead of `#ifdef`, as shown above.

### File and Directory Names

- All file and directory names should use UpperCamelCase, e.g. `MyDirectory/MyFile.cpp`. This keeps class names consistent with their file names.
- Exceptions:
    - `pch.h` for precompiled header files.
    - Files that require a different naming convention, e.g. `.gitignore` and `.clang-format`.
    - Binary libraries (.lib, .dll, etc.) should use lowerCamelCase, e.g. `hdAurora.dll`. Executables should use UpperCamelCase.

### Identifiers

- Use UpperCamelCase for type names, including classes: `MyAwesomeThing`.
- Use lowerCamelCase for variables and function names: `doSomethingCool()`.
- Use UPPER_CASE for macros: `MY_MACRO`. Note that macros are discouraged.
- Prefix constants and enumerators with "k": `kMyConstant`.
- Prefix member variables with an underscore: `_myMemberVariable`.
- Prefix pointer variables (including smart pointers) with a "p": `pMyPointer`.
- Use a verb prefix like `is` for boolean variables, e.g. `isEnabled`.
- Prefix global variables with "g": `gMyGlobalVariable`. Global variables should be rare, however.
- See the section below on prefixes for accessors.
- Other prefixes are optional.
- In general avoid the use of acronyms or abbreviations for identifiers. Some acceptable exceptions:
  - You may use `ID` (all caps) for "identifier" such as `objectID`.
  - You may use `Desc` for "description" such as `vertexDesc`.
  - You may use `it` for STL iterators, such as `auto it = cache.find(name)`.

### Indenting

Lines of code should be indented using four spaces, not tabs. This behavior is normally handled automatically by the IDE, and can be customized in Visual Studio:

1. Select *Tools | Options* to show the *Options* dialog.
2. Select *Text Editor | C/C++ | Tabs* from the tree.
3. Set *Tab size* and *Indent size* to 4.
4. Select *Insert spaces*.
5. Click OK.

Visual Studio displays the spacing type in the status bar, at the far right. Make sure it says *SPC*. If not, click it and select *Spaces*. You can also convert tabs to spaces with *Edit | Advanced | Untabify Selected Lines*.

### Line Length

- Each line of code, including comments, in a source file should not exceed 100 characters, except in rare cases.
- This allows two files to be viewed side-by-side (e.g. for comparing code) on a typical 16x9 monitor.
- [Visual Assist](https://www.wholetomato.com/) has an option to mark the right margin. In Visual Studio, use *Extensions | VAssistX | Visual Assist Options* (at bottom) and in the *Display* section, check the box *Display indicator after column* and put "100" in the box.

### Spacing Inside Expressions

- Should be done as follows: `if (AFunctionCall(x, y) == 0)`
- Note that there are no spaces just inside the parentheses, and there are spaces between keywords and operators.
- Avoided extended use of member access and pointer dereferencing, e.g.
  `thisPointer->someFunction().anotherFunction()->_someMember->yetAnotherFunction()`
  Instead store intermediate results, which will aid readability and debugging.

## Whitespace

Always remove all trailing whitespace, at the end of each line. Use [this Visual Studio extension](https://marketplace.visualstudio.com/items?itemName=MadsKristensen.TrailingWhitespaceVisualizer) to show trailing whitespace and automatically remove it on file save.

Do not use multiple consecutive empty lines.

See the rest of the standards for the proper use of empty lines, e.g. preceding comment lines with blank lines.

## Header Files

- Always include the license block (see below). Update the copyright year when updating the file.
- **In header files**, put `#pragma once` immediately after the license block (no empty line), followed by an empty line.
- **In CPP files**, if a precompiled header file is being used, put `#include "pch.h"` immediately after the license block (no empty line), followed by an empty line.
- Do not put `using` directives in public header files; that would pollute the client's namespace.
  - For example, something like `using namespace std;` must not appear in public header files.
  - A [type alias](#type-aliases) which also uses `using` is OK; this is different from a directive.
- Never use private header files outside their libraries, including `pch.h`. Such header files can only be used by code in the libraries to which they belong.
- Do not use deprecated header files from the C library. Instead, use the C++ equivalents. For example, use `<cmath>` instead of `<math.h>`. See [this clang-tidy check](https://clang.llvm.org/extra/clang-tidy/checks/modernize/deprecated-headers.html) for details.

### Including Header Files

- **Private headers:** Use quotes `"foo.h"` for header files that are not part of the public API for the current library, e.g. `#include "MyClass.h"`.
- **Public headers:** Use angle brackets `<foo.h>` for header files that are part of the public API for a library (including the current library), e.g. `#include <SomeLibrary/SomeHeader.h>`.
- **Aurora public headers:** Always treat Aurora public header files in the same way, with angle brackets and the fully qualified header file path starting with "Aurora", e.g. `#include <Aurora/Aurora.h>`.
- The exception to this is for Aurora header files that contain the declarations for the current CPP file: those should use quotes to distinguish them, e.g. within `Foo.cpp` the header file `Foo.h` which contains the declarations for the class or functions being implemented should be included as `#include "Aurora/Foo.h"`.
- Put header file includes in the following order:
  1. **Precompiled header file** (`pch.h`) for CPP files, immediately following the license block, followed by an empty line, as specified above.
  2. **Declaration header files** for the current CPP file as a group (usually just one), in quotes, followed by an empty line.
  3. **Public header files**, in alphabetical order with angle brackets, followed by an empty line. In general, these includes are only used in *public* header files; private header and CPP files should use a precompiled header file, as described below.
  4. **All other header files**, in alphabetical order.
- Put includes for public header files _that are outside the current library_ (e.g. Standard Library and system header files) in a precompiled header file (`pch.h`).
  - They will not change (add, remove, or modify) frequently in the course of developing that library, and so will benefit from being precompiled.
  - Avoid using those includes in private header or CPP files, in order to get the compile performance benefit.
  - Continue to put those includes in _public_ header files, as clients won't have access to `pch.h`.

### Including Header Files - Example

```c++
// Copyright 20XX Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once                // only for header files
// OR
#include "pch.h"            // only for CPP files
 
#include "MyClass.h"        // declarations for this CPP file: always with quotes
 
#include <APublicHeader.h>  // public headers: always with angle brackets
#include <ZPublicHeader.h>

#include "AClass.h"         // private headers: always with quotes
#include "ZClass.h"
```

### Member Order

Within a class, members should be declared in the following order:

- Members with the `public` access specifier, followed by `protected` and `private`.
- For each access specifier, specify members in the following order:
  1. **Types**: including nested classes and enumerations.
  2. **Functions**: in this order:
     - Static functions.
     - Constructors and destructors (lifetime management).
     - "get" accessors immediately preceding "set" accessors.
     - All other functions.
  3. **Variables**: roughly in decreasing order of importance. If variables require special ordering for performance or correctness (e.g. cache alignment), comment accordingly to prevent breaking changes by other developers.

Implementations of members in source files must be in the same order as their declarations.

## Logging

The Foundation library contains a Log.h header file with macros to support logging. These are not intended to be used for error handling. They accept a printf-style message that is written to the log. The messages are intended for other developers only, not for end users. The list below describes how and when to use each macro.

- **AU_INFO**: Use this to provide diagnostic information to developers. For example, progress reports or the results of successful file operations.
- **AU_WARN**: Use this to indicate to developers that a requested operation succeeded, but that some undesirable result may have occurred. For example, a resource exceeding a recommended size in memory.
- **AU_ERROR**: Use this to indicate to developers that a requested operation could not be completed as requested. For example, a requested data entry or file was not found and therefore could not be processed. This is usually followed by some form of error handling.
- **AU_FAIL**: In addition to reporting a message, this also aborts the application. Use this to indicate a catastrophic failure, due to an unhandled programming error. For example, a switch statement reaches an unexpected case. User action should not intentionally result in a failure.
- **AU_ASSERT**: This accepts a test condition that is expected to be true. If the condition evaluates to false, the failure is reported and the application is immediately aborted as with AU_FAIL. By convention, the test condition must not produce side effects. Use this throughout the code to ensure expected conditions are met, with any failures being the result of unhandled programming errors.
- **AU_ASSERT_DEBUG**: This is identical to AU_ASSERT except that it only executes in debug builds. Use this in performance-critical sections of the code, where it would be undesirable to test conditions in release builds. This should be used rarely, preferring AU_ASSERT.

Log.h also includes a singleton *Log* class for global configuration of logging, e.g. setting the level of logging that occurs at runtime.

## Error Handling

- Aurora currently does not have specific error handling standards. Until they are devised, use AU_ASSERT and AU_FAIL as described in the [Logging](#logging) section. These can be used to validate inputs and any other expected state in functions, which will help identify runtime errors that should be handled.
- **WIP:** Determine full standards for this, e.g. return values, exception handling, etc.

## CMake

Working with CMake scripts deserves its own set of complete standards. For now please consider [this small list of CMake "do's and don'ts"](https://cliutils.gitlab.io/modern-cmake/chapters/intro/dodonot.html) when you edit CMake scripts. In particular:

> Treat CMake as code: It is code. It should be as clean and readable as all other code.

In particular, use the same commenting standards for CMake scripts as for other code, as documented above.
