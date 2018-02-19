# ZPack
Embeddable library for creating archives with zstd compression.

```c_cpp
ZPack pack;
  
pack.open("/path/to/filename", /* trunicate? */true);
pack.packItem("special_item", "Text to write into item", "");
pack.packFile("/path/to/another/file");
pack.write();
pack.close();
```

```c_cpp
ZPack pack;
  
pack.open("/path/to/filename", /* trunicate? */true);

auto toStr = pack.extractStr("special_item");
pack.extractFile("file", "/path/to/destination");

pack.close();
```

Some examples may be found in main_test.cpp