#include "proto.h"
#include "asm.h"
#include <iostream>

/***********************************************************************
 *
 *		main.c
 *      Author: Mouloud Agaoua
 *
 *      Date: 2023/12/16
 *
 ************************************************************************/

int main(int argc, char **argv){

    if(argc < 2){
        std::cout << "usage: \n" << "./rigel68K [sourceFile] [output name] \n" << "example : ./rigel68K source.X68 output" << std::endl; 

        return -1;
    }


    char tempName[] = "./tempAXZE1D12398U.X68";
    std::string outputName;
    if(argc < 3){
        outputName = "genesis";
    }else{
        outputName = argv[2];
    }



    std::string workingDir = "./";

    int result = assembleFile(argv[1], tempName, outputName ,workingDir);
    if(result){
        std::cout << "usage: \n" << "./rigel68K [sourceFile]  \n" << std::endl; 
        return -1;
    }

    std::cout << "success" << std::endl;
    return 0;

}