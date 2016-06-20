/*
* Copyright (c)2015 Oasis LMF Limited
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.
*
*   * Neither the original author of this software nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*/

// Loss exceedence curve

/*

Because we are summarising over all the events we cannot split this into seperate processes and must do the summary in a single process
To achieve this we first output the results from summary calc to a set of files in a sub directory.
This process will then read all the *.bin files in the subdirectory and then compute the loss excedence curve in a single process

*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include "leccalc.hpp"

#ifdef __unix
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#include "../wingetopt/wingetopt.h"
#include "../include/dirent.h"
#else
#include <getopt.h>
#include <dirent.h>
#endif


using namespace std;


FILE *fout[] = { nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr };

int totalperiods = -1;
int maxsummaryid = 0;

bool isSummaryCalcStream(unsigned int stream_type)
{
	unsigned int stype = summarycalc_id & stream_type;
	return (stype == summarycalc_id);
}

std::map<int, std::vector<int> > event_to_periods;

bool operator<(const outkey2& lhs, const outkey2& rhs)
{
	if (lhs.period_no != rhs.period_no) {
		return lhs.period_no < rhs.period_no;
	}
	if (lhs.sidx != rhs.sidx) {
		return lhs.sidx < rhs.sidx;
	}else {
		return lhs.summary_id < rhs.summary_id;
	}
}

std::map<outkey2, float> agg_out_loss;
std::map<outkey2, float> max_out_loss;

void loadoccurence()
{
	FILE *fin = fopen(OCCURRENCE_FILE, "rb");
	if (fin == NULL) {
		cerr << "loadoccurence: Unable to open " << OCCURRENCE_FILE << "\n";
		exit(-1);
	}
	int date_algorithm;
	occurrence occ;
	int i = fread(&date_algorithm, sizeof(date_algorithm), 1, fin);
	i = fread(&totalperiods, sizeof(totalperiods), 1, fin);
	i = fread(&occ, sizeof(occ), 1, fin);
	while (i != 0) {		
		event_to_periods[occ.event_id].push_back(occ.period_no);
		i = fread(&occ, sizeof(occ), 1, fin);
	}

	fclose(fin);

}


//
//
//

inline void dolecoutputaggsummary(int summary_id, int sidx, float loss, const std::vector<int> &periods)
{
	outkey2 key;
	key.summary_id = summary_id;

	for (auto x : periods) {
		key.period_no = x;
		key.sidx = sidx;
		agg_out_loss[key] += loss;
		if (max_out_loss[key] < loss) max_out_loss[key] = loss;
/*
		auto iter = agg_out_loss.find(key);
		if (iter == agg_out_loss.end()) {
			agg_out_loss.insert(std::pair<outkey2, float>(key, loss));
			max_out_loss.insert(std::pair<outkey2, float>(key, loss));
		}
		else {
			agg_out_loss[key] += loss;
			if (max_out_loss[key] < loss) max_out_loss[key] = loss;
		}		
*/
	}
}

void processinputfile(unsigned int &samplesize)
{
	unsigned int stream_type = 0;
	int i = fread(&stream_type, sizeof(stream_type), 1, stdin);
	if (isSummaryCalcStream(stream_type) == true) {
		unsigned int summaryset_id;
		i = fread(&samplesize, sizeof(samplesize), 1, stdin);
		if (i == 1) i = fread(&summaryset_id, sizeof(summaryset_id), 1, stdin);
		if (i == 1) {
			while (i != 0) {
				summarySampleslevelHeader sh;
				i = fread(&sh, sizeof(sh), 1, stdin);
				std::map<int, std::vector<int> >::const_iterator iter;
				if (i) {
					iter = event_to_periods.find(sh.event_id);
					if (iter == event_to_periods.end()) {
						fprintf(stderr, "Event id %d not found in occurrence.bin\n", sh.event_id);
						exit(-1);
					}
					if (maxsummaryid < sh.summary_id) maxsummaryid = sh.summary_id;
				}
				while (i != 0) {
					sampleslevelRec sr;
					i = fread(&sr, sizeof(sr), 1, stdin);
					if (i == 0) break;
					if (sr.sidx == 0) break;
					//				dolecoutput1(sh.summary_id, sr.loss,iter->second);					
					if (sr.sidx != -2) {
						if (sr.loss > 0.0) dolecoutputaggsummary(sh.summary_id, sr.sidx, sr.loss, iter->second);
					}
				}
			}
		}
		else {
			std::cerr << "Stream read error\n";
			exit(-1);
		}
		return;
	}
	else {
		std::cerr << "Not a summarycalc stream\n";
		std::cerr << "invalid stream type: " << stream_type << "\n";
		exit(-1);
	}
}

void setinputstream(const std::string &inFile)
{
	if (freopen(inFile.c_str(), "rb", stdin) == NULL) {
		std::cerr << "Error opening " << inFile << "\n";
		exit(-1);
	}

}
void doit(const std::string &subfolder)
{
	std::string path = "work/" + subfolder;
	if (path.substr(path.length() - 1, 1) != "/") {
		path = path + "/";
	}
	unsigned int samplesize;
	if (subfolder.size() == 0) {
		processinputfile(samplesize);
	}
	else {
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(path.c_str())) != NULL) {
			while ((ent = readdir(dir)) != NULL) {
				std::string s = ent->d_name;
				if (s.length() > 4 && s.substr(s.length() - 4, 4) == ".bin") {
					s = path + ent->d_name;
					setinputstream(s);
					processinputfile(samplesize);
				}
			}
		}
	}

	outputAggWheatsheaf();
	outputAggFulluncertainty();
	outputAggWheatSheafMean(samplesize);
	outputAggSampleMean(samplesize);
	outputOccWheatsheaf();
	outputOccFulluncertainty();
	outputOccWheatSheafMean(samplesize);
	outputOccSampleMean(samplesize);


}


void openpipe(int output_id,const std::string &pipe)
{
	if (pipe == "-") fout[output_id] = stdout;
	else {
		FILE *f = fopen(pipe.c_str(), "wb");
		if (f != nullptr) fout[output_id] = f;
		else {
			std::cerr << "Cannot open pipe " << pipe << " for output\n";
			::exit(-1);
		}
	}
}


void help()
{
	fprintf(stderr, "-F Aggregate Full Uncertainty\n");
	fprintf(stderr, "-W Aggregate Wheatsheaf\n");
	fprintf(stderr, "-S Aggregate sample mean\n");
	fprintf(stderr, "-M Aggregate Wheatsheaf mean\n");
	fprintf(stderr, "-f Occurrence Full Uncertainty\n");
	fprintf(stderr, "-w Occurrence Wheatsheaf\n");
	fprintf(stderr, "-s Occurrence sample mean\n");
	fprintf(stderr, "-m Occurrence Wheatsheaf mean\n");
	fprintf(stderr, "-K workspace sub folder\n");
	exit(-1);
}

// 
int main(int argc, char* argv[])
{

	std::string subfolder;
	int opt;
	while ((opt = getopt(argc, argv, "F:W:M:S:K:f:w:s:m:")) != -1) {
		switch (opt) {
		case 'K':
			subfolder = optarg;
			break;
		case 'F':
			openpipe(AGG_FULL_UNCERTAINTY, optarg);
			break;
		case 'f':
			openpipe(OCC_FULL_UNCERTAINTY, optarg);
			break;
		case 'W':
			openpipe(AGG_WHEATSHEAF, optarg);
			break;
		case 'w':
			openpipe(OCC_WHEATSHEAF, optarg);
			break;
		case 'S':
			openpipe(AGG_SAMPLE_MEAN, optarg);
			break;
		case 's':
			openpipe(OCC_SAMPLE_MEAN, optarg);
			break;
		case 'M':
			openpipe(AGG_WHEATSHEAF_MEAN, optarg);
			break;
		case 'm':
			openpipe(OCC_WHEATSHEAF_MEAN, optarg);
			break;
		default:
			fprintf(stderr, "unknown parameter\n");
			::exit(EXIT_FAILURE);
		}
	}

	if (argc == 1) {
		fprintf(stderr, "Invalid parameters\n");
		help();
	}	
	initstreams();
	loadoccurence();
	doit(subfolder);
	return 0;

}
