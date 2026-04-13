[![DOI](https://zenodo.org/badge/984350098.svg)](https://doi.org/10.5281/zenodo.15537863)


# Selection Criteria

Implementation of selection criteria based on Hyperloglog and SuperMinHash sketches to speed up genomic similarity

## Installation (Linux: Ubuntu-24.04)

This tool requires the following dependencies. They can be installed via `apt-get`:
- make
- g++ 
- libz-dev

Now, clone this repository into target folder:
```
git clone https://github.com/AlvaroGuzmanChacon/Selection_Criteria
```

We need to install the following libraries:
- Seqan library. https://seqan.readthedocs.io/en/main/Infrastructure/Use/Install.html#infra-use-install or https://packages.seqan.de/
- Sketch library. https://github.com/dnbaker/sketch

To install them follow the steps below:

### Seqan library

```
wget https://packages.seqan.de/seqan-library/seqan-library-2.4.0.tar.xz
tar xvf seqan-library-2.4.0.tar.xz
cd seqan-library-2.4.0
sudo cp -r include/seqan /usr/include
sudo cp -r share/doc/seqan /usr/share/doc
```

### Sketch library

Clone the Sketch library using the following command inside the folder created when cloning this repository (by default its name is ``Selection_Criteria``):
```
git clone --depth 1 --branch v0.19.0 https://github.com/dnbaker/sketch
```

## Use

The implementation of the selection criteria is in the `src` folder, we also provide implementation for experiments to evaluate the selection criteria which are in the `experiments/src` folder. To compile the .cpp files use the Makefile provided, simply run `make`. The file `build_sketch.cpp` contains the sketch construction step. This program is used as follows:
```
./build/build_sketch -l filelist -t nthreads -a aux_memory -c criterion
```
Where
- `-l` option recieves a txt file containing the path to the .fna.gz files that will be processed. One line for every path.
- `-t` option recieves the number of threads to run the program.
- `-a` option recieves the additional memory, in bytes, that the criterion will use for each sequence.
- `-c` option recieves the criterion to use. The aviable options are `hll_a`, `hll_an` and `smh_a`.

Once the command is run, the primary hll sketches and auxiliary structures will be saved along the genomic .fna.gz files.

The `selection.cpp` file has the selection algorithm. This program is used as follows:
```
./build/selection -l filelist -t nthreads -h tau -a aux_memory -c criterion
```
Where
- `-l` option recieves a txt file containing the path to the .fna.gz files that will be processed. One line for every path.
- `-t` option recieves the number of threads to run the program.
- `-h` option recieves the similarity threshold `tau`.
- `-a` option recieves the additional memory, in bytes, that the criterion will use for each sequence.
- `-c` option recieves the criterion to use. The aviable options are `hll_a`, `hll_an` and `smh_a`.

## Datasets

The `datasets` folder has the datasets contains the manifest.zip files in the `Manifests` folder, along with a test dataset in the `test_influenzaA` folder. The `test_influenzaA` containts 10 compressed .fna files from the Influenza A GenBank dataset. These files are listed in the `test_influeza_filelist.txt` text file. To test the `build_sketch` or `selection.cpp` we can use this test datasets. Let's consider the next example:
```
./build/build_sketch -l test_influeza_filelist.txt -t 8 -a 256 -c hll_an
./build/selection -l test_influeza_filelist.txt -t 8 -h 0.8 -a 256 -c hll_an > results.txt
```
The first command would create the hll sketches of 256 bytes associated with the files specified in `test_influeza_filelist.txt`, using 8 threads. Then, the second command would use the `hll_an` criterion to try and find the pairs of sequences (between the 10 sequences on `test_influeza_filelist.txt`) with a Jaccard similarity greater than or equal to 0.8, using 8 threads and using the hll sketches of 256 bytes previously constructed.

## Experiments

We provide implementation of experiments to evaluate metrics such as time of comparisions and classification metrics. To run these experiments using a certain file list of sequences, first we have to build the associated sketches with the `build_sketch` program. These implementation are in the `experiments/src` folder. The implementation with the prefix `metrics`, return the True Positives, True Negatives, False Positives and False Negatives of the selection criteria based on the sketch given by the suffix (`hll` or `smh`). Similarly, the implementation with the prefix `time`, return the execution time, in seconds, that takes the selection criteria to retrieve similar pairs. Again, the suffix (`hll` or `smh`) indicates the selection criteria used (based on Hyperloglog or SuperMinHash). These programar are compiled when runing the Makefile provided.

As an example, then the program `metrics_hll` is used as follows:
```
./build/metrics_hll -l filelist -t nthreads -h tau -p hll_bits -n Taylors_order
```
Where
- `-l` option recieves a txt file containing the path to the .fna.gz files that will be processed. One line for every path.
- `-t` option recieves the number of threads to run the program.
- `-h` option recieves the similarity threshold `tau`.
- `-p` option recieves the number which determines the number of buckets of the auxiliar Hyperloglog sketch 2^p.
- `-n` option recieves the order of the Taylo's aproximation to use.

The output is the classification metrics of the `CB+hll_a` and `CB+hll_an` criterion, along with the `CB` criterion. Similarly, the `time_smh` program is used as follows:
```
./build/time_smh -l filelist -t nthreads -h tau -m buckets_smh
```
Where
- `-l` option recieves a txt file containing the path to the .fna.gz files that will be processed. One line for every path.
- `-t` option recieves the number of threads to run the program.
- `-h` option recieves the similarity threshold `tau`.
- `-m` option recieves number of buckets of the auxiliar SuperMinHash sketch m.

The output is the time it takes to retrieve the similar pairs using the `CB+smh_a` criterion, we also include the results of the `CB` criterion and with no criterion (baseline case).
