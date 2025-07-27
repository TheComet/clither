import sys
import os

this_dir = os.path.dirname(os.path.abspath(__file__))
if this_dir not in sys.path:
    sys.path.insert(0, this_dir)

from rb import rb_PrettyPrinter
from vec import vec_PrettyPrinter
from bmap import bmap_PrettyPrinter
from hmap import hmap_PrettyPrinter
from q import qw_PrettyPrinter, q16_16_PrettyPrinter, qa_PrettyPrinter, qwpos_PrettyPrinter, qwaabb_PrettyPrinter
from morton import morton_PrettyPrinter
from cmd import cmd_PrettyPrinter
from s import s_PrettyPrinter
from strspan import strspan_PrettyPrinter
from strview import strview_PrettyPrinter
from strlist import strlist_PrettyPrinter
from resource import resource_snake_PrettyPrinter, resource_sprite_PrettyPrinter, resource_pack_PrettyPrinter

def factories(val):
    if str(val.type).endswith("_rb *"):
        return rb_PrettyPrinter(val)
    if str(val.type).endswith("_vec *"):
        return vec_PrettyPrinter(val)
    if str(val.type).endswith("_bmap *"):
        return bmap_PrettyPrinter(val)
    if str(val.type).endswith("_hmap *"):
        return hmap_PrettyPrinter(val)
    if str(val.type).endswith("str *"):
        return s_PrettyPrinter(val)
    if str(val.type).endswith("strspan"):
        return strspan_PrettyPrinter(val)
    if str(val.type).endswith("strview"):
        return strview_PrettyPrinter(val)
    if str(val.type).endswith("strlist *"):
        return strlist_PrettyPrinter(val)
    if str(val.type) == "qw":
        return qw_PrettyPrinter(val)
    if str(val.type) == "q16_16":
        return q16_16_PrettyPrinter(val)
    if str(val.type) == "qa":
        return qa_PrettyPrinter(val)
    if str(val.type) == "qwpos":
        return qwpos_PrettyPrinter(val)
    if str(val.type) == "qwaabb":
        return qwaabb_PrettyPrinter(val)
    if str(val.type) == "morton":
        return morton_PrettyPrinter(val)
    if str(val.type) == "cmd":
        return cmd_PrettyPrinter(val)
    if str(val.type).endswith("resource_snake_part *"):
        return resource_snake_PrettyPrinter(val)
    if str(val.type).endswith("resource_sprite *"):
        return resource_sprite_PrettyPrinter(val)
    if str(val.type).endswith("resource_pack *"):
        return resource_pack_PrettyPrinter(val)


gdb.pretty_printers.append(factories)

import plot
import img
