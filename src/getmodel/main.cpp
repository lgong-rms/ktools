/*
* Copyright (c)2016 Oasis LMF Limited
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

#include <iostream>
#include <stdio.h>
#include "getmodel.hpp"
#include "../include/oasis.hpp"

#if defined(_WIN32) || defined(_WIN64)
#include "../wingetopt/wingetopt.h"
#endif
#ifdef __unix
#include <unistd.h>
#endif

void help()
{
	std::cerr 
        << "-I input filename\n"
		<< "-O output filename\n"
		<< "-d number of damage bins\n"
		<< "-n No intenisty uncertainty\n";

}

void doIt(int numDamageBins,  bool  hasIntensityUncertainty)
{

	getmodel cdf_generator;
	cdf_generator.init(numDamageBins, hasIntensityUncertainty);

	int event_id = -1;
	std::list<int> event_ids;
	while (fread(&event_id, sizeof(event_id), 1, stdin) != 0)
	{
		event_ids.push_back(event_id);
	}

	//! Send the events through in smaller batches.
	cdf_generator.doCdf(event_ids);
}

int main(int argc, char** argv)
{
	std::string inFile;
	std::string outFile;
    int numDamageBins = 100;
	bool noIntensityUncertainty = false;
	int opt;
	while ((opt = getopt(argc, argv, "hnI:O:d:M:")) != -1) {
		switch (opt) {
		case 'I':
			inFile = optarg;
			break;
		case 'O':
			outFile = optarg;
			break;
		case 'd':
			numDamageBins = std::stoi(optarg);
			break;
		case 'n':
			noIntensityUncertainty=true;
			break;
		case 'h':
			help();
			exit(EXIT_FAILURE);
		default:
			help();
			exit(EXIT_FAILURE);
		}
	}
	
	initstreams(inFile, outFile);
	doIt(numDamageBins, !noIntensityUncertainty);
	return 0;
}