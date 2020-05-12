## pybind11 Experiment

Compile datatype module using:
```bash
g++ -O3 -Wall -shared -std=c++11 -fPIC `python -m pybind11 --includes` datatype.cpp -o datatype`python3-config --extension-suffix`
```

Compile PnRdatabase module using:
```bash
g++ -I ../../json/include -O3 -Wall -shared -std=c++11 -fPIC `python -m pybind11 --includes` PnRdatabase-pybind11.cpp -o PnRdatabase`python3-config --extension-suffix` PnRDB.a ../../../googletest/googletest/mybuild/libgtest.so
```

First you need to recompile googletest using shared libraries:
```bash
cd path/to/googletest/googletest/mybuild
rm -rf ./*
cmake -DBUILD_SHARED_LIBS=ON ..
make
```

After a successful compile, then you can load up the database using C++ routines called from python (first change to the directory one above PnRDB)
```bash
cd ..
```

```python
from PnRDB.PnRdatabase import PnRdatabase
p = PnRdatabase( "./testcase_example", "switched_capacitor_filter", "switched_capacitor_filter.v", "switched_capacitor_filter.lef", "switched_capacitor_filter.map", "layers.json")
```
