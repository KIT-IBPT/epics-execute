Execute device support for EPICS
================================

The execute device support is a general-purpose software device support that can
be used to run any program from an EPICS IOC. Typically, this will be scripts
(e.g. Bash, Python), but it is perfectly possible to run binary programs as
well.

The possibility to use arbitrary scripts facilitates a wide range of possible
applications, like sending notifications, writing log files, calling web
services, querying databases, and many more.

The execute device support always runs program asynchronously, so a program will
never block operation of the IOC. Data can be passed from the IOC using
arguments (command-line parameters), environment variables, and a stream of
characters (supplied to the program on the standard input). Both the program's
exit code and its output can be passed back to the EPICS IOC.


Installing the device support
-----------------------------

The device support does not have any external dependencies except for
EPICS Base. It has been developed for EPICS Base 3.15, but it will most likely
work with recent versions of EPICS Base 3.14 or any version of EPICS Base 3.16
as well.

The path to the EPICS Base installation should be set by creating the file
`configure/RELEASE.local` and adding a line like the following:

```
EPICS_BASE = /path/to/epics/base
```

The device support must be compiled with a compiler that supports C++ 11. For
many compilers (including recent versions of GCC and Clang), C++ 11 support has
to be enabled explicitly. This can typically be done by creating the file
`configure/CONFIG_SITE.local` and adding the following line:

```
USR_CXXFLAGS += -std=c++11
```

After that, the device support can be compiled like any device support by
running `make`.


Adding the device support to an IOC
-----------------------------------

In order to use the device support in an IOC, a reference has to be added to the
`configure/RELEASE` file:

```
EXECUTE=/path/to/execute/device/support
```

In addition to that, it has to be added to the list of DBD files and libraries.
This is typically done in `xxxApp/src/Makefile`:

```
xxx_DBD += execute.dbd
xxx_LIBS += execute
```

These two steps should be sufficient for linking the IOC with the device
support.


Adding a command
----------------

The execute device support is built around the notion of commands. A command is
the entity that establishes a link between the various records involved in the
execution of a program.

A command is created by using the `executeAddCommand` command in the IOC's
startup script (typically `iocBoot/iocXXX/st.cmd`). The format of this IOC shell
command is:

`executeAddCommand("<command ID>", "<path to the program>", <no wait flag>)`

The `<command ID>` is the ID that identifies the command. It can be chosen
freely, but must be unique within the IOC (multiple instances of
`executeAddCommand` have to specifiy different IDs). It must only consist of
alphanumeric (ASCII) characters and the underscore.

The `<path to the program>` is the absolute path to the program that shall be
run. The specified string is *not* evaluated by a shell. This means that using
things like pipes, input or output redirection, etc. is not possible. If you
need such features, simply wrap the line in a shell script and specify the path
to the shell script instead.

The `<no wait flag>` specifies whether the device support waits for a program to
finish execution. If set to `0`, the device support waits for the program to
terminate and makes the program's exit code and output available to the
respective input records. As a consequence, it is not possible to run more than
one instance of the program in parallel (unless several command instances have
been defined for the same program). If set to `1` the device support does not
wait for the program to terminate. It simply forks the process without checking
whether the program executed successfully. This means that neither the exit code
nor the output can be made available via records. For this reason, this flag
should only be set if parallel execution of the same command instance is needed.

**Important security notice:** When writing a script that is executed by the
device support, keep in mind that command-line arguments and values of
environment variables (if made available through records) as well as the data
provided to the standard input can be controlled by a remote user. For this
reason, treat all these values as untrusted data and be sure to escape and / or
quote them properly when using them inside your script. In general, the same
considerations as when writing a CGI script executed by a webserver apply.


Supported records
-----------------

The execute device supports the `aai`, `aao`, `ao`, `bi`, `bo`, `longin`,
`longout`, `lsi`, `lso`, `mbbi`, `mbbo`, `mbbiDirect`, `mbboDirect`,
`stringin`, and `stringout` records. The `lsi` and `lso` records are only
supported when compiling against EPICS Base 3.15.1 or a newer release of
EPICS Base.

The `DTYP` that has to be specified for all records is `execute`. The format of
the address that has to be specified in the record's `INP` or `OUT` field
(depending on the record type) is:

`@<command ID> <type> <type dependent part>`

The `<command ID>` is the same ID that is specified when using
`executeAddCommand` in the IOC's startup file.

The `<type>` specifies the role of the record (e.g. command-line argument,
environment variable, exit code). The `<type dependent part>` depends entirely
on the specified type and is even missing for some types.

The exact address format for the various types and their meanings are discussed
in the rest of this section. Please note that not all records support all types.
Please refer to the sub-section of each address type for a list of the
corresponding record types.


### Setting program arguments (`arg`)

Arguments passed to the executed program (also known as command-line parameters)
are specified by using an address type of `arg`:

`@<command ID> arg <index>`

In this address, `<index>` is the numeric index of the respective argument. For
example, an `<index>` of `1` means that the respective value will be passed as
the first argument to the program. The number `0` cannot be used as (according
to POSIX conventions) this index is always used for passing the path to the
exexcutable.

This type of address can be used with the `aao`, `ao`, `bo`, `longout`, `lso`,
`mbbo`, `mbboDirect`, and `stringout` records. When using the `aao` record, the
element type (`FTVL`) must be `CHAR` or `UCHAR`. In case of the `ao` record, the
`VAL` field (floating point value) is used and no conversion applies. In case of
the `bo`, `mbbo`, and `mbboDirect` records, the `RVAL` field is used, so
conversion applies.

The argument is set when the respective record is processed and is then going to
be used the next time the command is run. It is possible to have more than one
record for the same command and argument index, but in this case the record that
is processed last is going to win. In order to avoid ambiguities, it is
suggested to avoid such scenarios and only specify one record per command and
argument index.

The number of arguments passed to a program is defined by the greatest argument
index that has been specified in any record defined for the command (assuming
the record has also been processed at least once). For example, if there only is
a record specifying an argument index of `3` (or the other records have not been
processed yet at the time when the command is run), three arguments (in addition
to the executable path) are passed to the program, and the arguments with the
indices 1 and 2 are set to the empty string. In order to avoid a varying number
of arguments being passed to the command, it is suggested to set each record's
`PINI` field to `YES`. This will ensure that all arguments are properly
initialized before running the command for the first time.

Example record definition for this address type:

```
record(longout, "$(P)$(R)Arg3") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) arg 3")
  field(VAL,  "-25")
  field(PINI, "YES")
}
```


### Setting environment variables (`env`)

Environment variables passed to the executed program are specified by using an
address type of `env`:

`@<command ID> env <variable name>`

In this address, `<variable name>` is the name of the environment variable that
shall be set. The specified environment variables supplement the environment of
the parent process (the IOC). This means that the forked process will have all
environment variables that are also defined in the IOC's environment. In
addition to that, it will have the environment variables defined by records. If
an environment variable is specified both by the IOC's environment and a record,
the value from the record takes precedence.

The `<variable name>` must only contain alphanumeric (ASCII) characters and the
underscore.

This type of address can be used with the `aao`, `ao`, `bo`, `longout`, `lso`,
`mbbo`, `mbboDirect`, and `stringout` records. When using the `aao` record, the
element type (`FTVL`) must be `CHAR` or `UCHAR`. In case of the `ao` record, the
`VAL` field (floating point value) is used and no conversion applies. In case of
the `bo`, `mbbo`, and `mbboDirect` records, the `RVAL` field is used, so
conversion applies.

The environment variable is set when the respective record is processed and is
then going to be used the next time the command is run. It is possible to have
more than one record for the same command and environment variable, but in this
case the record that is processed last is going to win. In order to avoid
ambiguities, it is suggested to avoid such scenarios and only specify one record
per command and environment variable.

An environment variable is only set once the corresponding record has been
processed. For this reason, it is suggested to set each record's `PINI` field to
`YES`. This will ensure that all environment variables are properly initialized
before running the command for the first time and a variable will never be
missing from the command's environment.

Example record definition for this address type:

```
record(ao, "$(P)$(R)MyVar") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) env MY_VAR")
  field(VAL,  "7.3")
  field(PINI, "YES")
}
```


### Supplying data to the standard input (`stdin`)

Data may be passed to the standard input of the run program by using an address
type of `stdin`:

`@<command ID> stdin`

`@<command ID> stdin null-terminated`

This type of address can be used with the `aao`, `lso`, and `stringout` records.
When using the `aao` record, the element type (`FTVL`) must be `CHAR` or
`UCHAR`. Using the `aao` record is the only means of passing any data that may
contain null bytes. Such data cannot be set using the `lso` or `stringout`
records, and it cannot be passed as an argument or an environment variable
either.

The `null-terminated` option is optional. It only has an effect when using the
`aao` record. The effect of this option is that only data before the first
null byte is passed to the standard input, just like it is done for environment
variables and arguments or when using the `lso` or `stringout` records.

The data is only made available to the standard input once the corresponding
record has been processed. However, setting `PINI` to `YES` is mainly useful
when using the `stringout` record because there is no proper way for setting
the `VAL` field of an `aao` or `lso` record’s value as part of the record
definition (you can use an `lso` record’s `DOL` field with a constant JSON link
though).

Example record definition for this address type:

```
record(aao, "$(P)$(R)StdIn") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) stdin")
  field(FTYP, "CHAR")
  field(NELM, "1024")
}
```


### Reading the exit code (`exit_code`)

The exit code from a program's last execution can be retrieved by using an
address type of `exit_code`:

`@<command ID> exit_code`

This type of address can be used with the `bi`, `longin`, `mbbi`, and
`mbbiDirect`. When using the `bi`, `mbbi`, or `mbbiDirect` record, the `RVAL`
field is used, so conversion applies.

When the record is processed, it reads the exit code returned by the program the
last time the command was run. If the command has not bee run yet, the record's
value is set to 0. If the program did not terminate normally, but was killed by
a signal, the record's value is set to -1. If the program could not be run
because a system call failed (e.g. the process could not be forked), the
record's value is set to -2.

Processing of the record must be triggered after the program has terminated. The
easiest way of achieving this is placing a forward link from the record
triggering the command to be run to the record reading the exit code. It is
possible to have several records reading the exit code (e.g. a `bi` record
used for a general success or error flag and a `longin` record for getting more
detailed status information).

The exit code cannot be read if a command's no-wait flag has been set. Defining
a record for such a command will result in an error during record
initialization.

Example record definition for this address type:

```
record(bi, "$(P)$(R)Status") {
  field(DTYP, "execute")
  field(INP,  "@$(CMD) exit_code")
  field(ZNAM, "Success")
  field(ONAM, "Error")
}
```


### Reading the output (`stdout` or `stderr`)

Data that is written by the program to the standard output or the standard error
output, can be retrieved by using an address type of `stdout` or `stderr`
respectively:

`@<command ID> stdout`

`@<command ID> stderr`

This type of address can be used with the `aai`, `lsi` and `stringin` records.
When using the `aai` record, the element type (`FTVL`) must be `CHAR` or
`UCHAR`. Using the `aai` record is the only means of (completely) retrieving
data that may contain null bytes. Such data cannot be retrieved using the
`lsi` or `stringin` records (any data following the first null byte is going to
be discarded in this case).

Processing of the record must be triggered after the program has terminated. The
easiest way of achieving this is placing a forward link from the record
triggering the command to be run to the record reading the program's output. It
is possible to have several records reading the same output (e.g. two records
reading from `stdout`), but usually there is little use for that.

The output cannot be read if a command's no-wait flag has been set. Defining a
record for such a command will result in an error during record initialization.

Example record definition for this address type:

```
record(stringin, "$(P)$(R)StdOut") {
  field(DTYP, "execute")
  field(INP,  "@$(CMD) stdout")
}
```


### Running a command (`run`)

A command is run by processing a record that uses an address type of `run`:

`@<command ID> run`

`@<command ID> run wait`

The `wait` flag is optional. It defines the processing behavior of the record.

Usually (without the `wait` flag), the record changes its value to 1 and starts
execution of the command when being processed. Once the command finishes
execution, the record is automatically processed again and changes its value
to 0. This means that the record can be monitored in order to know whether the
command has finished execution. Please note that when the record points to
another one via a forward link, the target record is going to be processed
twice: the first time when the execution of the command starts and the second
time when the execution of the command finishes. This should not be an issue
when the target record reads the exit code or the program's output. In this
case, the target record might read old data (from the previous execution) during
its first processing, but will always read the newest data during its second
processing.

If the command's no-wait flag is set, the record's value is never changed to 1
and it simply starts the execution of the command and resets the value to 0.

If the `wait` flag is set in the address, the record is processed differently.
The record's value also changes to 1 when the execution of the command is
started. However, the record does not finish processing immediately. Instead,
the `PACT` flag is set in order to indicate that record processing will
complete asynchronously. This means that no forward links are processed at this
point and no Channel Access monitors are notified, so monitors will not see the
value change. Once the command's execution completes, the record completes the
processing, setting its value to `0` and clearing the `PACT` flag. After this,
the forward link is processed as well.

Setting the `wait` flag in the record address might be desirable when using
`ca_put_callback` in order to trigger processing of the record. In this case,
the client starting the processing will only be notified once the command's
execution has completed. If the `wait` flag is not set, the client is notified
immediately after starting execution of the command.

The `wait` flag in the record address must not be set if the command's no-wait
flag is set. Setting the `wait` flag for a record that refers to a command that
has been defined with its no-wait flag set, will result in an error during
record initialization.

The `run` address type can only be used with the `bo` record. The record's value
is changed when being processed, but any value specified by the user does not
have any effect.

Unless the command's no-wait flag has been set, only one run of the command can
be triggered in parallel. When using a single `bo` record, the device support
ensures this. Triggering processing of the record before the command has
finished execution will have no effect (except for restoring the record's value
to 1, if it has been changed). However, if there is more than one record for
triggering execution of the same command, triggering execution through the
second record while an execution triggered through the first record is still
running will result in an error. For this reason, it is suggested to only use a
single record for each command.

Example record definition for this address type:

```
record(bo, "$(P)$(R)Run") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) run")
}
```


Known issues
------------

On macOS (at least on macOS 10.12), it can happen that the execution of a
command is started, but the actual executable is never invoked and the forked
process is stalled indefinitely.

This problem is caused by a bug in the loader (dyld): When using `fork()` in a
multi-threaded program, it can happen that the loader tries to acquire a mutex
in the forked process that had been held by another thread in the process that
called `fork()`. This happens even when only using async signal safe functions
in the forked process. Whenn inspecting the forked process in the debugger, the
stack trace looks like this:

```
frame #0: 0x00007fffe14a8c22 libsystem_kernel.dylib`__psynch_mutexwait + 10
frame #1: 0x00007fffe1593dfa libsystem_pthread.dylib`_pthread_mutex_lock_wait + 100
frame #2: 0x00007fffe1591519 libsystem_pthread.dylib`_pthread_mutex_lock_slow + 285
frame #3: 0x00007fffe1376118 libdyld.dylib`dyldGlobalLockAcquire() + 16
frame #4: 0x0000000115ce4f96 dyld`ImageLoaderMachOCompressed::doBindFastLazySymbol(unsigned int, ImageLoader::LinkContext const&, void (*)(), void (*)()) + 54
frame #5: 0x0000000115ccc86d dyld`dyld::fastBindLazySymbol(ImageLoader**, unsigned long) + 90
frame #6: 0x00007fffe1376282 libdyld.dylib`dyld_stub_binder + 282
frame #7: 0x0000000109cb6320 libexecute.dylib
```

Unfortunately, there is no workaround for this bug that can be implemented in
the code. The only known workaround is disabling lazy binding when linking the
device support library. This can be achieved by adding the following line to
`configure/CONFIG_SITE.local`:

```
USR_LDFLAGS += -bind_at_load
```


Example application
-------------------

This section presents a simple example that shows how the three parts (executed
program, IOC startup script, and records) work together.

We assume that there is a simple shell script `/usr/local/bin/greeter.sh` with
the following content:

```
#!/bin/bash

case "$language" in
  0)
    echo -n "Hello $1"
    ;;
  1)
    echo -n "Hallo $1"
    ;;
  *)
    echo "Unsupported language ID: $language" >&2
    exit 1
esac
```

As you can see, this script expects a name as its first (and only) argument and
also expectes an environment variable with the name `language` that defines the
language of the greeting. Please note that we only use arguments and environment
variables in quotes. This is important to ensure that their values cannot be
used to inject commands.

In the IOC's startup script (`st.cmd`), we define the command and load the
corresponding record definitions:

```
executeAddCommand("CMD0", "/usr/local/bin/greeter.sh", 0)
dbLoadDatabase("db/greeter.db", "P=Test:,R=Greeter:,CMD=CMD0)
```

Finally, we define a few records in the record file (`greeter.db`):

```
record(stringout, "$(P)$(R)Name") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) arg 1")
  field(VAL,  "Anonymous")
  field(PINI, "YES")
}

record(mbbo, "$(P)$(R)Language") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) env language")
  field(VAL,  "0")
  field(PINI, "YES")
  field(ZRST, "English")
  field(ZRVL, "0")
  field(ONST, "German")
  field(ONVL, "1")
  field(TWST, "French")
  field(TWVL, "2")
}

record(bo, "$(P)$(R)Run") {
  field(DTYP, "execute")
  field(OUT,  "@$(CMD) run")
  field(ZNAM, "Idle")
  field(ONAM, "Running")
  field(FLNK, "$(P)$(R)ExitCode")
}

record(bi, "$(P)$(R)ExitCode") {
  field(DTYP, "execute")
  field(INP,  "@$(CMD) exit_code")
  field(ZNAM, "OK")
  field(ONAM, "Error")
  field(FLNK, "$(P)$(R)Greeting")
}

record(stringin, "$(P)$(R)Greeting") {
  field(DTYP, "execute")
  field(INP,  "@$(CMD) stdout")
  field(FLNK, "$(P)$(R)ErrorMessage")
}

record(stringin, "$(P)$(R)ErrorMessage") {
  field(DTYP, "execute")
  field(INP,  "@$(CMD) stderr")
}
```

After starting the IOC, we can run the command by writing to the `PROC` field of
the `Run` record:

```
caput Test:Greeter:Run.PROC 0
```

As a result, the records for the exit code, the greeting and the error message
should have been updated:

```
caget Test:Greeter:{ExitCode,Greeting,ErrorMessage}
Test:Greeter:ExitCode          OK
Test:Greeter:Greeting          Hello Anonymous
Test:Greeter:ErrorMessage
```

Now, we change the name and run the command again:

```
caput Test:Greeter:Name Peter
caput Test:Greeter:Run.PROC 0
```

The output should have changed now:

```
caget Test:Greeter:{ExitCode,Greeting,ErrorMessage}
Test:Greeter:ExitCode          OK
Test:Greeter:Greeting          Hello Peter
Test:Greeter:ErrorMessage
```

Now, we also change the language:

```
caput Test:Greeter:Language German
caput Test:Greeter:Run.PROC 0
```

The greeting is now printed in German:

```
caget Test:Greeter:{ExitCode,Greeting,ErrorMessage}
Test:Greeter:ExitCode          OK
Test:Greeter:Greeting          Hello Peter
Test:Greeter:ErrorMessage
```

Finally, we change the language to French:

```
caput Test:Greeter:Language French
caput Test:Greeter:Run.PROC 0
```

As this language is not supported by the shell script, we expect an error:

```
caget Test:Greeter:{ExitCode,Greeting,ErrorMessage}
Test:Greeter:ExitCode          Error
Test:Greeter:Greeting
Test:Greeter:ErrorMessage      Unsupported language ID: 2\n
```

Please note the `\n` in the error message. As we did not suppress the output of
a newline character (by using `echo -n`), it has been included in the message
printed by the shell script.


Copyright / License
-------------------

This EPICS device support is licensed under the terms of the
[GNU Lesser General Public License version 3](LICENSE-LGPL.md). It has been
developed by [aquenos GmbH](https://www.aquenos.com/) on behalf of the
[Karlsruhe Institute of Technology's Institute of Beam Physics and Technology](https://www.ibpt.kit.edu/).

This device support contains code originally developed for the s7nodave device
support. aquenos GmbH has given special permission to relicense these parts of
the s7nodave device support under the terms of the GNU LGPL version 3.
