# Change history

## HEAD

    - Docker based installation added

    - Asynchronous use of pipes by test bench added

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
