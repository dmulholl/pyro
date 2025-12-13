# Embedded Files

Files placed in this `embed` directory are embedded into the compiled Pyro binary.

You can access these files in Pyro using the `std::pyro::load_embedded_file()` function.
For example, to access the `embed/foo/bar.txt` file, you can run:

    import std::pyro;
    var content = pyro::load_embedded_file("foo/bar.txt");

Embedded files are automatically compressed using the LZ4 compression algorithm and automatically uncompressed on loading.
