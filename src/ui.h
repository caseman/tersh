/*
The Tersh UI consists of a hierarchy of widgets nested as follows

+-----------------------------------------------------・・・
|  Root* (one per window)
|  +--------------------------------------------------・・・
|  |  Tab Bar
|  +--------------------------------------------------・・・
|
|  +--------------------------------------------+  +--・・・
|  |  Shell*                                    |  | Shell
|  |                                            |  |
|  |  +------------------------------------+-+  |  .
|  |  |  Output container                  |△|  |  .
|  |  |                                    +-+  |  .
|  |  |  +-------------------------------+ | |  |
|  |  |  |  Job Output N*                | | |  |
|  |  |  |                               | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Job Status           ▷ |  | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Virtual Terminal       |  | | |  |
|  |  |  |  ~                         ~  | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  +-------------------------------+ | |  |
|  |  |                 ...                | |  |
|  |  |  +-------------------------------+ | |  |
|  |  |  |  Job Output 2*                | | |  |
|  |  |  |                               | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Job Status           ▷ |  | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Virtual Terminal       |  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  ~                         ~  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  +-------------------------------+ | |  |
|  |  |  +-------------------------------+ | |  |
|  |  |  |  Job Output 1*                | | |  |
|  |  |  |                               | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Job Status           ▷ |  | | |  |
|  |  |  |  +-------------------------+  | | |  |
|  |  |  |  |  Virtual Terminal       |  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  ~                         ~  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  |                         |  | | |  |
|  |  |  |  +-------------------------+  | +-+  |
|  |  |  +-------------------------------+ |▽|  |
|  |  +------------------------------------+-+  |
|  |                                            |
|  |  +--------------------------------------+  |
|  |  | Command Line*                        |  |
|  |  |                                      |  |
|  |  |  +--------------------------------+  |  |
|  |  |  | Top Prompt                     |  |  |
|  |  |  +--------------------------------+  |  |
|  |  |  | > Command Line Editor          |  |  |
|  |  |  |                                |  |  |
|  |  |  +--------------------------------+  |  |
|  |  |  | Command Assist                 |  |  |
|  |  |  +--------------------------------+  |  |
|  |  +--------------------------------------+  |
|  |                                            |
|  +--------------------------------------------+
|
+----------------------------------------------------- ・・・

* These are strictly container widgets and aren't visible themselves

Some of these widgets are further composed of other smaller widgets. Widgets
are designed to be lightweight so they can be used liberally to help automate
flexible layout and modularize event handling.

Virtual terminals are also lightweight even though they are each fully
independent terminal emulators, so that having many accumulate over time is
not a problem.  Even if many jobs are still executing in parallel, their
terminals can be updated efficiently.

Event propagation is similar to systems like the DOM, where inner child
widgets receive events, then either handle them or bubble them up to their
parents.

A running job consists of a process group: one or more processes executed from
a command invocation. Multiple processes occur when commands are composed of a
chain or pipeline.

When a job is running, its i/o is attached to the virtual terminal. By
default the last jobs' vterm will have the focus, receiving keyboard
events. However, some keyboard events are not forwarded to the attached
process, e.g., CTRL+Z, CMD+key combos, and these would bubble up to their
parents, or parent's parents to be handled.

In the case of CTRL+Z, it would ultimately bubble up to the shell widget,
which will send SIGSTOP
*/

