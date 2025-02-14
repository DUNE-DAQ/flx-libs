local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local ns = "dunedaq.flxlibs.felixcardcontroller";
local s = moo.oschema.schema(ns);

// Object structure
local felixcardcontroller = {
    boolean : s.boolean("boolean",
                      doc="A boolean"),

    uint4  : s.number("uint4", "u4",
                      doc="An unsigned of 4 bytes"),

    uint8 : s.number("uint8", "u8",
                      doc="An unsigned of 8 bytes"),

    string : s.string("String",
                       doc="A string field"),

    array : s.sequence("Array", self.uint4, doc="list of numbers"),

    regvalpair : s.record("RegValPair", [
    s.field("reg_name", self.string, "",
                doc="The name of a register"),
    s.field("reg_val", self.uint8, 0,
                doc="The value of a register")
    ]),

    reglist : s.sequence("RegList", self.string,
                doc="A list of registers"),

    regvallist : s.sequence("RegValList", self.regvalpair,
                doc="A list of registers and values"),

    link : s.record("Link", [
        s.field("link_id", self.uint4, doc="Link identifier"),
        s.field("enabled", self.boolean, doc="Indicate whether the link is enabled"),
        s.field("dma_desc", self.uint8, doc="DMA channel"),
        s.field("superchunk_factor", self.uint4, doc="Superchunk factor")
    ], doc=""),

    links : s.sequence("LinksList", self.link,
                doc="A list of links"),
 
    logical_unit: s.record("LogicalUnit", [
        s.field("log_unit_id", self.uint4, doc="Logical unit identifier"),
        s.field("emu_fanout", self.boolean, doc="Toggle emulator on/off"),
        s.field("links", self.links, doc="List of links in the logical unit"),
        s.field("ignore_alignment_mask", self.array, [5], doc="Elinks to ignore when checking alignment."),
    ], doc=""),
    
    logical_units : s.sequence("LogicalUnitList", self.logical_unit,
                doc="A list of logical units"),

    conf: s.record("Conf", [
    s.field("card_id", self.uint4, 0,
            doc="Physical card identifier (in the same host)"),

    s.field("logical_units", self.logical_units,
            doc="Superlogic regions of selected card"),

    ], doc="Upstream FELIX CardController DAQ Module Configuration"),

    getregister: s.record("GetRegisters", [
        s.field("card_id", self.uint4, 0,
                doc="Physical card identifier (in the same host)"),
        s.field("log_unit_id", self.uint4, doc="Logical unit identifier"),
        s.field("reg_names", self.reglist,
                        doc="A list of registers")
    ], doc="Register access parameters"),

    setregister: s.record("SetRegisters", [
        s.field("card_id", self.uint4, 0,
                doc="Physical card identifier (in the same host)"),
        s.field("log_unit_id", self.uint4, doc="Logical unit identifier"),

        s.field("reg_val_pairs", self.regvallist,
                        doc="A list of registers and values to set")
    ], doc="Register access parameters"),

    getbitfield: s.record("GetBFs", [
        s.field("card_id", self.uint4, 0,
                doc="Physical card identifier (in the same host)"),
        s.field("log_unit_id", self.uint4, doc="Logical unit identifier"),
        s.field("bf_names", self.reglist,
                        doc="A list of bitfields")
    ], doc="Bitfield access parameters"),

    setbitfield: s.record("SetBFs", [
        s.field("card_id", self.uint4, 0,
                doc="Physical card identifier (in the same host)"),
        s.field("log_unit_id", self.uint4, doc="Logical unit identifier"),
        s.field("bf_val_pairs", self.regvallist,
                        doc="A list of bitfields and values to set")
    ], doc="Bitfield access parameters"),

};

moo.oschema.sort_select(felixcardcontroller, ns)
