# Specification for the Kubeka Continuous Delivery tool

## OVERVIEW
**Kubeka** is a small, simple CD tool for performing automated actions, such
as testing, deployments, etc.

> **GOAL**
>
> Specify CD workflows in a format simpler than current formats, using
> linting to prevent runtime errors.
>

## MECHANISM
The input file is a *Simple List* of `nodes`. A `node` **must be** one of
[the predefined types](#nt). A node contains a [collection of
variables](#nv) which are set in the input file.

The input files are parsed into an *Execution Tree* and the tree [is
linted](#sc)
to detect as many errors as possible before it is evaluated. A linting
failure prevents the evaluation from taking place.

A node is invoked by either another node or by a `signal` (not to be
confused with POSIX signals).

Once invoked, a node **must perform** one, and **only one** of the following
actions::

1. Execute a command
2. Emit a signal
3. Invoke an ordered list of other nodes

### Sanity Checks {#sc}
On tree creation, prior to evaluation, some sanity checks are applied:

- All substitutions must resolve (no circular resolutions)
- IDs cannot be duplicated
- No invocation (via `JOBS` or `JOBS[]` or `EMIT`) can be mutually
  recursive
- **All nodes** have a set of [**mandatory variables**](#mv)
    that must be defined

## INPUT FILE FORMAT
A simple one, using the following pattern:
```
    [<node-type>]
    variable = value
    ...
```
Some variables are required (`ID`, `MESSAGE`), while others are read-only
(`_WORKING_PATH`).


## NODE TYPES {#nt}
1. Job nodes: an atomic unit of execution, with the type `job`. Cannot
    be used as a starting node in any workflow.
2. Entry-point nodes: a node that serves as the start of a workflow.
   Currently only nodes of type `periodic` allow starting a workflow. Future
   planned entrypoint nodes include webhooks for popular git SaaS providers.
3. A node must EITHER execute a shell command OR invoke another node OR
   emit a signal. If a node is to do nothing, use `EXEC = /bin/true`.

## NODE VARIABLES{#nv}
Each node can have unlimited variables attached to it. Variables are
dynamically scoped and are resolved up the call-tree. IOW, if a variable
is referred to within a node, and that node hasn't an entry in the symbol
table for that variable, then the ancestors are searched for that variable
until one is found.

Variables are set using `variable = some value`, on a single line. Variables
are substituted using `$(variable)`.

 1. Variables with a leading underscore cannot be set (constants)
 2. Variables [in all uppercase](#tuv) can be read/written, but are used by
    the CD engine itself (for example, setting JOBS to tell the CD engine
    what jobs to run).
 3. All other non-numbered variable patterns are up for grabs by the user
 [^1].
 4. All `$(symbol)` is parsed as a variable substitution
 5. All `$(symbol text ...)` is parsed as a builtin command, the results of
    which are substituted. See "BUILTIN COMMANDS" in the docs.
 6. [All variables are arrays](#avaa).

### The uppercase variables {#tuv}
 All uppercase variables are used by the CD engine. Excepting the variables
 with a leading underscore, they can all be read and written by the user.

 The uppercase variables are as follows:

| Variable | Used as  |
| ------------ | ---------------|
| `ID`            |  Sets a **unique** name for the node.
| `_WORKING_PATH` |  Constant containing the execution directory
| `EMIT`          |  The signal to generate on successful completion
| `HANDLES`       |  Specifies a signal to start on
| `MESSAGE`       |  The message that will be logged when node is invoked
| `EXEC`          |  A command that will be executed when node is invoked
| `ROLLBACK`      |  A command that will be executed on node failure
| `JOBS`          |  An ordered array of jobs to execute
| `LOG`           |  A comma-separated list of [logging keywords](#lk)

### Mandatory variables {#mv}
For **all nodes** the following variables **must** be defined:

  - `ID`
  - `MESSAGE`
  - **Only one** of the following: `EXEC` or  `JOBS` or `EMIT`

### All variables are arrays {#avaa}
All variables are arrays. No variable is scalar (non-array). Setting a
value to a variable without any index sets the first element of that
array. Using a variable without specifying an index substitutes the first
element of that array.

Array indices **must be integer literals only**. You cannot, at this time,
use a variable as an index into an array[^2].

| Variable Usage | Explanation |
| ------------  | ---------------|
| `$(var)`      |  Shorthand for `$(var[0])`
| `var = name`  |  Shorthand for `var[0] = name`
| `$(var[#])`   |  The number of elements in the array
| `$(var[\*])`  |  A single string containing all the
|               |  elements separated by a single space
|               |  character, for example `el-1 el-2 ...`
| `$(var[@])`   |  A single string containing all the
|               |  elements in array syntax, for
|               |  example `[ el-1, el-2, .... el-n ]`

## Logging keywords {#lk}
The `LOG` variable specifies what to log. The user specifies a comma
separated list of *logging keywords* (default 'exec, exit-error, info`):

| Keyword | Logging behaviour |
| ------- | ---------------- |
| `exec`        | Log all exec output (stdout + stderr)
| `exit-error`  | Log the exit code when the exit code is non-zero
| `exit-success`| Log the exit code when the exit code is zero
| `entry`       | A message when invocation enters a node
| `exit`        | A message when invocation leaves a node
| `info`        | The identifier and message of the node during invocation


## EXAMPLE
Following is an example kubeka file, for a hypothetical deployment of a
piece of software that has some `ROLLBACK` requirement.

```
--- deployment-name.kubeka ---

# Mechanism:
# All the *.kubeka files are parsed into memory as a flat collection of
# nodes. An index is written to /etc/kubeka/index.
#
# Nodes are then invoked at periodic intervals (`periodic` nodes only) or on
# receipt of a (`job` nodes only) SIGNAL (not to be confused with POSIX
#
# *** (PLANNED: github/gitlab/gitea webhook nodes) ***
#
# Each node has *at least* the following fields:
#     type        The type of node (periodic, job, etc)
#     symtab      A symbol table[1]
#     parent      A pointer to a parent node (invocation only, for symbol
#                 resolution)
#     children    An array of child nodes (invocation only, for clean up)
#
# A node is cloned, and only the clone is invoked (this keeps the original
# symbol table intact for future invocations of that node). The 'cloning'
# is not really a clone, as it will use $(JOBS[]) to populate all the
# children, and will need a parent to attach to.
#
# The result is a **tree** of nodes, with the root node getting invoked,
# and recursively invoking (or emitting) other nodes.
#
# Invocation is performed by shelling out and executing $(EXEC), or by
# executing all elements of $(JOBS) in order (sanity check ensures that
# only one of EXEC, JOBS or EMIT will be set).
#
# Symbol lookup is performed recursively towards the root of the tree
# during invocation. This means that variables are dynamically scoped.
#
# [1] Symbol table:
#  The symtab (symbol table) is a dynamically scoped table of symbols
#  implemented as a hashtable of {symbol:string[]}, ie a symbol matches an
#  array of strings.
#

[periodic]
ID = git checker
INTERVAL = 5m     # This will run every five minutes
JOBS[] = [ Check changes ]
git_origin = ssh://some-origin/some-repo.git
git_branch = master


[job]
ID = My First deployment
_WORKING_PATH = /some/other/path  # Must result in an error, reserved
END_SIGNAL = Signal to be emitted when this job completes (any string)
START_SIGNAL = Signal to wait for to start this job

# Some variables set by the user
appname = $(env_read APPNAME)
build_artifacts =
build_artifacts = $(build_artifacts) bin/program
build_artifacts = $(build_artifacts) info/release-info.txt
target_directory = /var/www/$(appname)

# Finally, the ordered list of jobs to execute for this job
JOBS[] = [ Full deployment ]

# All these can be in various separate files, actually...

[job]
ID = Full deployment
MESSAGE = Performing a full deployment
JOBS[] = [ build, upgrade, Done ]

[job]
ID = build
MESSAGE Starting build process
JOBS[] = [ Check changes, checkout, Build thing, test, package ]

[job]
ID = upgrade
MESSAGE = Performing upgrade
JOBS[] = [ pre-deployment, Upgrade DB, Copy files]

[job]
ID = Check changes
MESSAGE = Checking if sources changed, will continue only if sources changed
LOG = # Suppress all messages
EXEC =
EXEC = $(EXEC) if [ `git  ls-remote $(git_origin) heads/master | cut -f 1 ` == $(current HASH)]; then
EXEC = $(EXEC)    exit -1;
EXEC = $(EXEC) else
EXEC = $(EXEC)    exit 0;

[job]
ID = checkout   # Comments work like this
MESSAGE = A single long line message up to 8kb
ROLLBACK = # Nothing to do if this fails
EXEC = git clone $(git_origin) && git checkout $(git_branch) $(_WORKING_PATH)

[job]
ID = Build thing
MESSAGE = Building $(_JOB) for $(_DEPLOYMENT) with parameter $1
ROLLBACK = # Nothing to do if this fails
EXEC = make $1

[job]
ID = test
MESSAGE = Testing $(_DEPLOYMENT)
ROLLBACK = # Nothing to do if this fails
EXEC = test.sh

[job]
ID = package
MESSAGE = Packaging $(_DEPLOYMENT)
EXEC = tar -zcvf $(_TMPDIR)/$(env_read APPNAME).tar.gz $(build_artifacts)

[job]
ID = pre-deployment
MESSAGE = Deploying to $(target_directory)
WORKING_DIR = $(target_directory)
EXEC = systemctl stop && tar -zxvf $(env_read APPNAME)

[job]
ID = Upgrade DB
MESSAGE = Upgrading database
WORKING_DIR = $(target_directory)
ROLLBACK = $(exec ./rollback.sh)
EXEC = db/upgrade.sh

[job]
ID = Copy files
MESSAGE = Copying files to $(target_directory)
EXEC = cp -Rv $(artifacts) $(target_directory)

[job]
ID = Done
MESSAGE = Setting finished variables
HASH = $(exec git log -1 | head -n 1 | cut -f 1 -d \  )

```


## Footnotes
[^1]: Numbered variables are reserved for future use (*RFU*), with respect
  to adding support for parameters to nodes.
[^2]: At time of writing there is no support for iteration on an array. As
such, there is no good reason to allow using variables as indices, while
there is a *very* good reason to disallow this, *viz* bounds-checking
failure will be possible at runtime. With literals all bounds-checking can
be performed prior to evaluation.

