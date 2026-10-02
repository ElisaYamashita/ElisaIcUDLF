/* <Effectiveness.hpp>
 *
 * Effectiveness class header file
 *
 * @Authors: Lucas Pascotti Valem <lucasvalem@rc.unesp.br>
 *           Daniel Carlos Guimarães Pedronette <daniel@rc.unesp.br>
 *
 ***********************************************************************************
 *
 * This file is part of Unsupervised Distance Learning Framework (UDLF).
 *
 * UDLF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * UDLF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with UDLF.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef EFFECTIVENESS_HPP
#define EFFECTIVENESS_HPP

#include <iostream>
#include <memory>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>

#if defined(USE_OPTANE_MATRIX) || defined(USE_OPTANE_OTHERS)
#include <memkind_allocator.h>
#endif

class Effectiveness {
    public:
            
            #ifdef USE_OPTANE_OTHERS
            using MemkindAlloc = libmemkind::static_kind::allocator<int>;
            using MemkindVector = std::vector<int, MemkindAlloc>;
            #endif

            Effectiveness(long int& n_in,
                          #ifdef USE_OPTANE_OTHERS
                          MemkindVector & rkLists_in,
                          #else
                          std::vector<int>& rkLists_in,
                          #endif
                          std::vector<std::string>& imgList_in);
            //Recall
            void fillRecallsMap(std::map<int, float>& recalls, std::string recallsToCompute);
            //MAP
            float computeMAPMeasure();
            //Precision
            void fillPrecisionsMap(std::map<int, float>& precisions, std::string precisionsToCompute);
            //Files
            void readClassesFile(std::string classFile);
            //Given an image, return its class name
            std::string getClass(int x);

    private:
            //Recall
            float computeRecall(int recallAt);
            void computeShowResults(int recallAt);
            void computeAvgArray();
            float computeFinalRecall();
            //Precision
            float computePrecision(int k);
            //MAP
            float computeMAP(int paramNK);
            float computeAveragePrecision(int qId, int d, int offset);
            void startMAPByClass();
            void endMAPByClass();
            //Generic
            int getClassSize(std::string classname);

            //Variables
            const long int& n;
            
            #ifdef USE_OPTANE_OTHERS
            const MemkindVector & rkLists;
            #else
            const std::vector<int>& rkLists;
            #endif
            const std::vector<std::string>& imgList;
            std::map<std::string, std::string> classes;
            std::map<std::string, unsigned int> classesSize;
            std::map<std::string, float> mapByClass;

};

#endif // EFFECTIVENESS_HPP
