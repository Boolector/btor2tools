Btor2Tools
===============================================================================

The Btor2Tools package provides a generic parser and tools for the BTOR2 format.

For a more detailed description of the BTOR2 format, refer to  
*BTOR2, BtorMC and Boolector 3.0.* Aina Niemetz, Mathias Preiner, Clifford Wolf,
and Armin Biere. CAV 2018.

Download
-------------------------------------------------------------------------------

  The latest version of Btor2Tools can be found on GitHub:
  https://github.com/boolector/btor2tools

Build
-------------------------------------------------------------------------------

From the Btor2Tools root directory configure and build as follows:
```
./configure.sh
make
```
For more build configuration options of Btor2Tools, see `configure.sh -h`.

All binaries (btorsim, catbtor) are generated into directory `btor2tools/bin`,
and all libraries (libbtor2parser.a, libbtor2parser.so) are generated into
directory `btor2tools/build`.


Usage
-------------------------------------------------------------------------------

### BTOR2 Parser

Btor2Parser is a generic parser for the BTOR2 format.

```
Btor2Parser* parser;
Btor2LineIterator it;
Btor2Line* line;

parser = btor2parser_new ();
if (!btor2parser_read_lines (reader, input_file))
{
  // parse error
  const char *err = btor2parser_error (parser);
  // error handling
}
// iterate over parsed lines
it = btor2parser_iter_init (parser);
while ((line = btor2parser_iter_next (&it)))
{
  // process line
}
btor2parser_delete (parser);

```

For a simple example on how to use the BTOR2 parser, refer to `src/catbtor.c`.  
For a more comprehensive example, refer to function `parse_model()` in
`src/btorsim/btorsim.c`.


### BtorSim

BtorSim is a witness simulator and checker for BTOR2 witnesses.

For a list of command line options, refer to `btorsim -h`.  
For examples and instructions on how to use BtorSim, refer to
`examples/btorsim`.

### Catbtor

Catbtor is a simple tool to parse and print BTOR2 files. It is mainly used for
debugging purposes.

For a list of command line options, refer to `catbtor -h`.
