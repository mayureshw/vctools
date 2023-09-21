# Change history

## HEAD

    - Docker based installation added

    - Asynchronous use of pipes by test bench added

    - Randomized simulation modes with CEP technique to detect violation to
      properties added. (A major change.)

    - README.md replaced with manual.pdf due to increase in contents

    - A number of changes to prepare for asynchronous code generation

## 221011

    - Added operators >> << >o> <o< < ^ ~& ~| ~^

    - Added a unique sequence number for each event to help correlate logs

    - Wide unsigned integer support (>64 bit) of a compile time configurable
      maximum width (see Makefile.conf for configurability) was added

    - A bug with Not operator was fixed

    - Type system for operators, pipes, storage, values etc was made
      specification driven using opfactory and a generated header opf.h

## 220917

    Initial release
