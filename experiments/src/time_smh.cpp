#include "sketch/sketch.h"
#include <fstream>
#include <omp.h>
#include <iostream>
#include <seqan/seq_io.h>
#include <include/metrictime2.hpp>
#include <include/criteria_sketch.hpp>


uint64_t canonical_kmer (uint64_t kmer, uint k = 31)
{
	uint64_t reverse = 0;
	uint64_t b_kmer = kmer;

	kmer = ((kmer >> 2)  & 0x3333333333333333UL) | ((kmer & 0x3333333333333333UL) << 2);
	kmer = ((kmer >> 4)  & 0x0F0F0F0F0F0F0F0FUL) | ((kmer & 0x0F0F0F0F0F0F0F0FUL) << 4);
	kmer = ((kmer >> 8)  & 0x00FF00FF00FF00FFUL) | ((kmer & 0x00FF00FF00FF00FFUL) << 8);
	kmer = ((kmer >> 16) & 0x0000FFFF0000FFFFUL) | ((kmer & 0x0000FFFF0000FFFFUL) << 16);
	kmer = ( kmer >> 32                        ) | ( kmer                         << 32);
	reverse = (((uint64_t)-1) - kmer) >> (8 * sizeof(kmer) - (k << 1));

	return (b_kmer < reverse) ? b_kmer : reverse;
}

void sketch_file (std::vector<uint64_t> &mh_vector, std::string filename, int k)
{
	sketch::SuperMinHash<> smh(mh_vector.size()-1);
	seqan::SeqFileIn seqFileIn;
	if (!open(seqFileIn, filename.c_str ()))
	{
		std::cerr << "ERROR: Could not open the file " << filename << ".\n";
		return;
	}

	seqan::CharString id;
	seqan::IupacString seq;

	while (!atEnd (seqFileIn))
	{
		try {
			seqan::readRecord(id, seq, seqFileIn);
		}
		catch (seqan::ParseError a) {
			break;
		}

		uint64_t kmer = 0;
		size_t bases = 0;
		for (size_t i = 0; i < length(seq); ++i)
		{
			uint8_t two_bit = 0;//(char (seq[i]) >> 1) & 0x03;
			bases++;

			switch (char (seq[i]))
			{
				case 'A': two_bit = 0; break;
				case 'C': two_bit = 1; break;
				case 'G': two_bit = 2; break;
				case 'T': two_bit = 3; break;
				case 'a': two_bit = 0; break;
				case 'c': two_bit = 1; break;
				case 'g': two_bit = 2; break;
				case 't': two_bit = 3; break;
					  // Ignore kmer
				default: two_bit = 0; bases = 0; kmer = 0; break;
			}

			kmer = (kmer << 2) | two_bit;
			kmer = kmer & ((1ULL << (k << 1)) - 1);

			if (bases == k)
			{
				//s->add (XXH3_64bits ((const void *) &kmer, sizeof (uint64_t)));
				smh.addh(canonical_kmer (kmer));
				bases--;
			}
		}
	}
	for(int i=0;i<mh_vector.size();i++){
		mh_vector[i] =smh.h_[i];
	}
	close (seqFileIn);
}

void load_file_list (std::vector<std::string> & files, std::string & list_file, std::string path = "")
{
	std::string line;

	if (list_file.empty ())
	{
		std::cerr << "No input file provided\n";
		exit (-1);
	}

	std::ifstream file (list_file);

	if (!file.is_open ())
	{
		std::cerr << "No valid input file provided\n";
		exit (-1);
	}

	while (getline (file, line)) files.push_back (path + line);
	file.close();
}


// ----------------------------------------------------------
// ------------------------- MAIN ---------------------------
// ----------------------------------------------------------

int main(int argc, char *argv[])
{
	// Paso 0: Cargar parámetros ##############################
	std::vector<std::string> files;

	std::string list_file = "";
	uint threads = 8;
	const uint k = 31;
	const uint sketch_bits = 14;
	float threshold = 0.9;
	int mh_size = 8;
  int total_rep = 1;

	char c;

	while ((c = getopt(argc, argv, "xl:t:h:m:R:")) != -1)
	{
		switch (c) {
			case 'x':
				std::cout << "Usage: -l -t -h -m\n";
				return 0;
				break;
			case 'l':
				list_file = std::string (optarg);
				break;
			case 't':
				threads = std::stoi (optarg);
				break;
			case 'h':
				threshold = std::stof (optarg);
				break;
			case 'm':
				mh_size = std::stoi (optarg);
				break;
      case 'R':
				total_rep = std::stoi (optarg);
				break;
			default:
				break;
		}
	}

	// Inicializar variables:
	omp_set_num_threads (threads);
	load_file_list (files, list_file);

	std::cout << list_file << ";build_smh;" << threshold << ";";
	TIMERSTART(construccion)
	std::vector<std::pair<std::string, double>> card_name (files.size ());
	std::map<std::string, std::shared_ptr<sketch::hll_t>> name2hll;
	std::map<std::string, std::vector<uint64_t>> name2mhv;
	int add_mem = 8*mh_size;

	for (size_t i_processed = 0; i_processed < files.size (); ++i_processed)
	{
		std::string filename = files.at (i_processed);
		std::vector<uint64_t> v(mh_size);
		name2hll[filename] = std::make_shared<sketch::hll_t> (sketch_bits);
		name2mhv[filename] = v;
	}


	#pragma omp parallel for schedule(dynamic)
	for (size_t i_processed = 0; i_processed < files.size (); ++i_processed)
	{
		std::string filename = files.at (i_processed);
		// Cargar .hll de tamaño 2^14 ya antes creados
		name2hll[filename] = std::make_shared<sketch::hll_t>(filename + ".hll");
		// Cosntruir nuevos sketches auxiliares (hll y mh)
		sketch_file (name2mhv[filename], filename, k);

		auto c = name2hll[filename]->report ();
		card_name.at (i_processed) = std::make_pair (filename, c);
	}

	TIMERSTOP(construccion)
	std::cout << ";m:" << mh_size << "\n";

	// Ordenamos los pares de card_name según la estimación (hll's de tamaño p=14).
	std::sort (card_name.begin (), card_name.end (),
			[](const std::pair<std::string, double> &x,
				const std::pair<std::string, double> &y)
			{
			return x.second < y.second;
			});



	int n_rows = 1, n_bands = 1;
	for (int band=1; band <=mh_size; band++){
		if (mh_size % band != 0) continue;
		float P_r = 1.0-pow(1.0-pow(threshold, (float)mh_size/band), (float)band);
		if (P_r >= 0.95){
			n_bands = band;
			n_rows = mh_size / band;
			break;
		}
	}


	std::string out[files.size()];

  for (int rep=0; rep<total_rep; rep++)
  {
	// CRITERIO SMHa
	std::cout << list_file << ";smh_a;" << threshold << ";";
	TIMERSTART(criterio_smh)
	#pragma omp parallel for schedule(dynamic)
	for (size_t i_processed = 0; i_processed < card_name.size () - 1; ++i_processed){
		std::string out_str;

		std::string fn1 = card_name[i_processed].first;
		size_t e1 = card_name[i_processed].second;

		size_t k = i_processed + 1;
		for (; k < card_name.size (); ++k)
		{
			std::string fn2 = card_name[k].first;
			size_t e2 = card_name[k].second;
			if(e2 == 0)continue;

			bool smha_selects = smh_a(name2mhv[fn1], name2mhv[fn2], n_rows, n_bands);
			if (!smha_selects) continue;

			double t = name2hll[fn1]->union_size(*name2hll[fn2]);
			double jacc14 = ((double)e1+(double)e2-t) / t;
			if (jacc14 >= threshold){
				out_str += fn1 + " " + fn2 + " " + std::to_string(jacc14) + "\n";
			}
		}
		out[i_processed] = out_str;
	}
	TIMERSTOP(criterio_smh)
	std::cout << ";r:" << n_rows << "_" << "b:" << n_bands << "\n";


	// CRITERIO CB+SMHa
	std::cout << list_file << ";CB+smh_a;" << threshold << ";";
	TIMERSTART(criterio_CBsmh)
	#pragma omp parallel for schedule(dynamic)
	for (size_t i_processed = 0; i_processed < card_name.size () - 1; ++i_processed){
		std::string out_str;

		std::string fn1 = card_name[i_processed].first;
		size_t e1 = card_name[i_processed].second;

		size_t k = i_processed + 1;
		for (; k < card_name.size (); ++k)
		{
			std::string fn2 = card_name[k].first;
			size_t e2 = card_name[k].second;
			if(e2 == 0)continue;

			bool CB_selects = CB(threshold, e1, e2);
			if (!CB_selects) break;

			bool smha_selects = smh_a(name2mhv[fn1], name2mhv[fn2], n_rows, n_bands);
			if (!smha_selects) continue;

			double t = name2hll[fn1]->union_size(*name2hll[fn2]);
			double jacc14 = ((double)e1+(double)e2-t) / t;
			if (jacc14 >= threshold){
				out_str += fn1 + " " + fn2 + " " + std::to_string(jacc14) + "\n";
			}
		}
		out[i_processed] = out_str;
	}
	TIMERSTOP(criterio_CBsmh)
	std::cout << ";r:" << n_rows << "_" << "b:" << n_bands << "\n";
  }
	return 0;
}
