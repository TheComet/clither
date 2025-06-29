import gdb
from curses.ascii import isgraph

def groups_of(iterable, size, first=0):
    first = first if first != 0 else size
    chunk, iterable = iterable[:first], iterable[first:]
    while chunk:
        yield chunk
        chunk, iterable = iterable[:size], iterable[size:]

class HexDump(gdb.Command):
    def __init__(self):
        super (HexDump, self).__init__ ('xxd', gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)

        addr = gdb.parse_and_eval(argv[0]).cast(
            gdb.lookup_type('void').pointer())
        if len(argv) == 2:
             try:
                 bytes = int(gdb.parse_and_eval(argv[1]))
             except ValueError:
                 raise gdb.GdbError('Byte count numst be an integer value.')
        else:
             bytes = 500

        inferior = gdb.selected_inferior()

        align = gdb.parameter('xxd-align')
        width = gdb.parameter('xxd-width')
        if width == 0:
            width = 16

        mem = inferior.read_memory(addr, bytes)
        pr_addr = int(str(addr), 16)
        pr_offset = width

        if align:
            pr_offset = width - (pr_addr % width)
            pr_addr -= pr_addr % width
        start=(pr_addr) & 0xff;


        print ('                ' , end="")
        print ('  '.join(['%01X' % (i&0x0f,) for i in range(start,start+width)]) , end="")
        print ('   ' , end="")       
        print (''.join(['%01X' % (i&0x0f,) for i in range(start,start+width)]) )

        for group in groups_of(mem, width, pr_offset):
            print ('0x%x: ' % (pr_addr,) + '   '*(width - pr_offset), end="")
            print (' '.join(['%02X' % (ord(g),) for g in group]) + \
                '   ' * (width - len(group) if pr_offset == width else 0) + '  ', end="")    
            print (' '*(width - pr_offset) +  ''.join(
                [chr( int.from_bytes(g, byteorder='big')) if isgraph( int.from_bytes(g, byteorder='big')   ) or g == ' ' else '.' for g in group]))
            pr_addr += width
            pr_offset = width

class HexDumpAlign(gdb.Parameter):
    def __init__(self):
        super (HexDumpAlign, self).__init__('xxd-align',
                                            gdb.COMMAND_DATA,
                                            gdb.PARAM_BOOLEAN)
    set_doc = 'Determines if xxd always starts at an "aligned" address (see xxd-width'
    show_doc = 'Hex dump alignment is currently'

class HexDumpWidth(gdb.Parameter):
    def __init__(self):
        super (HexDumpWidth, self).__init__('xxd-width',
                                            gdb.COMMAND_DATA,
                                            gdb.PARAM_INTEGER)
    set_doc = 'Set the number of bytes per line of xxd'
    show_doc = 'The number of bytes per line in xxd is'

HexDump()
HexDumpAlign()
HexDumpWidth()

