//
// base64decode.cpp
//
// This sample demonstrates the Base64Decoder and StreamCopier classes.
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//

#include <fstream>
#include <iostream>

#include "Poco/Base64Decoder.h"
#include "Poco/StreamCopier.h"

using Poco::Base64Decoder;
using Poco::StreamCopier;

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "usage: " << argv[0] << ": <input_file> <output_file>" << std::endl
              << "       read base64-encoded <input_file>, decode it and write the result to <output_file>" << std::endl
              << "       read from stdin if <input_file> is '-', decode it and write the result to stdout" << std::endl;
    return 1;
  }

  if (argv[1] == std::string("-"))
  {
    Base64Decoder decoder(std::cin);
    StreamCopier::copyStream(decoder, std::cout);
  }
  else
  {
    std::ifstream istr(argv[1]);
    if (!istr)
    {
      std::cerr << "cannot open input file: " << argv[1] << std::endl;
      return 2;
    }

    std::ofstream ostr(argv[2], std::ios::binary);
    if (!ostr)
    {
      std::cerr << "cannot open output file: " << argv[2] << std::endl;
      return 3;
    }

    Base64Decoder decoder(istr);
    StreamCopier::copyStream(decoder, ostr);

    if (!ostr)
    {
      std::cerr << "error writing output file: " << argv[2] << std::endl;
      return 4;
    }
  }

  return 0;
}
