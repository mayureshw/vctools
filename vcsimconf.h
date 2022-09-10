#ifndef _VCSIMCONF_H
#define _VCSIMCONF_H

// By default following logs are always kept enabled and they appear on stdout:
//
//   - Module entry and exit with input and output parameters respectively
//   - Writes to the system pipes
//
// This header has some configurable options. To disable them just comment them
// out. There are separate log files for various aspects. All log files follow
// a terse format, such as : separated values, which makes it easy to analyze
// logs or write tools around them.
//
// If you want a high performance simulation keep all the logging options
// disabled. In favour of performance these options are made compile time
// options so that when they are disabled they don't cause any overhead.
//
// Do not forget to run "make" after any change to these options!

// Enable pipe logging in file pipes.log. All push and pop operations are
// logged. Push on full and pop from empty are logged identidiably.
#define PIPEDBG

// Enable operator logging in file ops.log. On update-ack event, the inputs and
// outputs of an operator are logged. For certain operators that do not have
// update-ack, their sample-ack is logged.
#define OPDBG

// Petri net logging in file petri.log. Every transition, addition and
// deduction of tokens from every place and 'waiting transitions' are logged.
// For more details see README.md of the petrisimu package.
#define PNDBG

// CEP based verification tools are triggered if this option is set. See
// README.md of the ceptool for more details.
#define USECEP

// Generate data path dot file for each module, with name <modulename>_CP.dot
// #define GEN_CPDOTFILES

// Generate data path dot file for each module, with name <modulename>.dot
// #define GEN_DPDOTFILES

// Generate Petri net dot file as petri.dot
// #define GEN_PETRIDOTFILE

// Generate json file representation of Petri net in petri.json
// #define GEN_PETRIJSONFILE

// Generate pnml representation of the Petri net in petri.pnml
// #define GEN_PETRIPNMLFILE

// ********** Usually you won't need to change beyond this **********

#ifdef USECEP
#   define PN_USE_EVENT_LISTENER
#endif

// In case one wishes to override the stock classes
#define PNTRANSITION PNTransition
#define PNPLACE PNPlace

#endif
