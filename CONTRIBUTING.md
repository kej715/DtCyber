# Contributing
Contributions to the *DtCyber* repository of new features, enhancements, and bug
fixes are welcome. To contribute code and related artifacts, follow these basic
steps:

* Create a fork of the repository in GitHub.
* Create a local clone of your new fork.
* Implement and test your changes and additions in your local clone, and ensure that
they conform to the coding style discussed below.
* Commit your changes and additions to your local clone.
* Push the changes to your fork in GitHub.
* Use GitHub to submit a pull request to the main *DtCyber* repository, requesting to
merge your changes into main repo.

## Coding Style
Code must conform to the coding style established by Tom Hunter, the original creator of *DtCyber*. Review the existing source code to become familiar with the style. The
essential rules are:

**Indentation**. Spaces must be used for indentation, and each level of indentation
is four spaces. Tabs are not accepted.

**Braces** Braces are indented the same number of spaces as the body of code they surround. For example, an *if* statement looks like:

>     if (condition)
>         {
>         stmt1;
>         stmt2;
>         .
>         .
>         .
>         stmtn;
>         }
>     else
>         {
>         stmt1;
>         stmt2;
>         .
>         .
>         .
>         stmtn;
>         }

a private function definition looks like:

>     static type functionName(type paramName)
>         {
>         decl1;
>         decl2;
>         
>         stmt1;
>         stmt2;
>         .
>         .
>         .
>         stmtn;
>         }

**switch statements** Braces are indented four spaces from the level of the
*switch* keyword, and *case* keywords are indented at the same level as the
*switch* keyword. For example:
    
>     switch (exp)
>         {
>     case l1:
>         stmt1;
>         break;
>     case l2:
>         stmt2;
>         break;
>     default:
>         stmt3;
>         break;
>         }

**variable names** Variable names and formal parameter names begin with a lower case
letter, and *camel case* is used to indicate where words begin and end within names.
Underscores are **not** used in variable names. For example, a conformant variable
name is `conformantVariableName`, and a conformant parameter name is `parameterName`.
Names such as `VariableName`, `variable_name`, and `parameter_name` are
**non-conformant**.

**field names** Names of fields in structs and unions begin with a lower case
letter, and *camel case* is used to indicate where words begin and end within names.
Underscores are **not** used in field names. For example, a conformant field
name is `conformantFieldName`. Names such as `FieldName` and `field_name`
are **non-conformant**.

**constant names** Constant names begin with an upper case letter. Camel case may be used to indicate where words begin and end, or a constant name may be entirely upper
case, and underscores may be used to indicate where words begin and end. For example,
conformant constant names are `ConstantName` and `CONSTANT_NAME`. Non-conformant
constant names are `constantName` (it looks like a variable name) and `constant_name`
(it begins with a lower case letter).

**typedef names** Names of types defined by *typedef* declarations begin with an
upper case letter, and *camel case* is used to indicate where words begin and end
within them. Underscores are **not** used in *typedef* names. For example:

>     typedef struct feiBuffer
>         {
>         u32 in;
>         u32 out;
>         u8  data[MaxByteBuf];
>         } FeiBuffer;

**enum members** Member names in enums begin with an upper case letter, and camel case
is used to indicate where words begin and end. For example:

>     typedef enum
>         {
>         StDsa311OutSOH = 0,
>         StDsa311OutENQ,
>         StDsa311OutDLE1,
>         StDsa311OutDLE2,
>         StDsa311OutETB1,
>         StDsa311OutETB2,
>         StDsa311OutCRC1,
>         StDsa311OutCRC2
>         } Dsa311OutputState;

**function names** Like variable names, function names begin with a lower case
letter, and *camel case* is used to indicate where words begin and end within them.
In addition, the left-most word of a function name should reflect the name of the
module containing it. For example, the names of functions in the module `init.c`
begin with *init*, the names of functions in the module `operator.c` begin with *op*,
the names of function in the module `npu_bip.c` begin with *npuBip*, etc.

**public and private names** All functions and global variables should be declared
explicitly as *static* unless they are intended to be referenced outside of the
module in which they are defined. All public (i.e., externally referenceable) names
are declared in the header file `proto.h`.

**whitespace and parenthesis** A single blank separates the *for*, *if*, *switch*,
and *while* keyword from the open parenthesis following it. **NO** whitespace occurs
between a function name and the open parenthesis that follows it in a declaration or
call.

## Formatting Tools
Use tools provided in the [se-tools](se-tools) directory to facilitate conformance
with the required coding style. For example, the shell script `se-tools/style.sh`
accepts a file name, applies [`uncrustify`](https://github.com/uncrustify/uncrustify)
to it, and then post-processes the output to produce an output stream conforming to
the indentation and whitespace rules discussed above. `uncrustify` is a widely used
tool for formatting source code according to predefined styling rules. The file, `se-tools/style.cfg`, provides most of the indentation and whitespace rules needed by `uncrustify` to support the required *DtCyber* coding style. In addition, `se-tools/pp` is a simple C program that can be applied to the output of `uncrustify` to post-process *switch* statements so that they conform to *DtCyber* coding style too.
