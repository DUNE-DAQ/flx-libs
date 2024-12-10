# This module facilitates the generation of FLX card controller apps

# Set moo schema search path
from dunedaq.env import get_moo_model_path
import moo.io
moo.io.default_load_path = get_moo_model_path()

# Load configuration types
import moo.otypes

moo.otypes.load_types('flxlibs/felixcardcontroller.jsonnet')

# Import new types
import dunedaq.flxlibs.felixcardcontroller as flx

from daqconf.core.app import App, ModuleGraph
from daqconf.core.daqmodule import DAQModule
from detchannelmaps._daq_detchannelmaps_py import *

#===============================================================================
def get_cardcontroller_app(
        nickname,
        card_id=0,
        emulator_mode=False,
        ignore_alignment_mask=([], []),
        host="localhost",
        ru_desc=None) -> App:
    '''
    Here an entire application controlling one physical FLX card is generated. 
    '''

    if ru_desc.kind != 'flx':
        raise ValueError(f"Felix controller app creation requrested for RU of kinf {ru_desc.kind}")

    # Get Elink infos for every SLR on this physical card.
    slrs = {}
    for stream in ru_desc.streams:
        if not stream.parameters.slr in slrs:
            slrs[stream.parameters.slr] = []
        slrs[stream.parameters.slr].append(stream.parameters.link)

    # Sort elinks in each SLR
    for slr in slrs:
        slrs[slr].sort()

    # RS: Not needed with HW map
    #if len(elinks) != 2:
    #    raise Exception("elinks needs to be supplied two lists, one for each logical unit.")

    # Define modules
    modules = []
    lus = []

    # Create FelixCardControllerModule plugin configs
    for slr in slrs:
        elinks = []
        for l in slrs[slr]:
            elinks.append(flx.Link(link_id=l, enabled=True, dma_desc=0, superchunk_factor=12))
        # if enable_firmware_tpg:
            # elinks.append(flx.Link(link_id=5, enabled=True, dma_desc=0, superchunk_factor=64))
        lus.append(flx.LogicalUnit(log_unit_id=slr, emu_fanout=emulator_mode, links=elinks, ignore_alignment_mask=ignore_alignment_mask[slr]))

    # Create modules
    modules += [DAQModule(name = nickname, 
                          plugin = 'FelixCardControllerModule',
                          conf = flx.Conf(card_id = card_id, logical_units = lus)
                             )]

    mgraph = ModuleGraph(modules)
    flx_app = App(modulegraph=mgraph, host=host, name=nickname)

    return flx_app

