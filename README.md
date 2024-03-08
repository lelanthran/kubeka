# kubeka
Lightweight Continuous Deployment with simple configuration

# Overview
There is a in-flux specification document somewhere in this directory. A
better approach is to read this document to get an idea of whether or not
this is for you.

The goal of this tool is to make it easy to define jobs to execute in a
continuous deployment process.


## Quickstart
Each action to perform is a node in a tree. Nodes are assigned mandatory
types and unique `ID`s by the user using a `key = value` format, with one
keypair per line.

lets look at an example node that defines a job to run. First, a quick look
at a node without any comments.
```
[job]
ID = Perform a git clone
MESSAGE = Cloning the repo
EXEC = git clone $<MYREPO> the-repo/
ROLLBACK = rm -rf the-repo
```

Now a little explanation in the comments describing waht's going on.
```
# The node type is, in this example, `[job]`. Unfortunately the **only
# way** a node of type `[job]` can be run is from *another node*, so
# this single node can't really do anything unless some other node calls
# it.
[job]

# If `ID`s clash, no big deal - `kubeka` will report the clash and refuse
# to run.
ID = Perform a git clone

# During execution, a message will be logged when a node is being either
# evaluated or executed. This is that message. It is mandatory.
MESSAGE = Cloning the repo

# The actual command to run. Note the usage of `$<MYREPO>` to perform
# a variable substitution using `kubeka` variables. More on that later.
EXEC = git clone $<MYREPO> the-repo/

# In case the $<EXEC> failed, the variable $<ROLLBACK>, which is optional,
# can be used to specify a command to run to rollback the failed and/or
# incompleted $<EXEC> action.
ROLLBACK = rm -rf the-repo
```

It's pretty simple:
- Each line sets a variable using `key = value`
- Variables are substituted using `$<VARIABLE>`.
- Some variables are mandatory, some are optional and some are read-only.

# Node types
There are three types of nodes:
<table>
    <thead> <th><td> Node type </td><td> Usage </td></th> </thead>
    <tbody>
        <tr>
            <td><tt> job </tt></td>
            <td> </td>
        </tr>
    </tbody>
</table>

| --------- | ----- |
| `job`     | You basic node that can run a command, or specify other nodes
</table>
