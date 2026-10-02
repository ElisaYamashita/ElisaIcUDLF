/* <Cprr.cpp>
 *
 * CPRR method implementation file
 *
 * @Authors: Lucas Pascotti Valem <lucasvalem@rc.unesp.br>
 *           Daniel Carlos Guimarães Pedronette <daniel@rc.unesp.br>
 *
 *******************************************************************************************************************
 *
 * Cartesian Product of Ranking References (CPRR) is presented in the paper:
 *   L. P. Valem and D. C. G. Pedronette.
 *   "Unsupervised similarity learning through cartesian product of ranking references for image retrieval tasks."
 *   2016 29th SIBGRAPI Conference on Graphics, Patterns and Images (SIBGRAPI).
 *   http://dx.doi.org/10.1109/SIBGRAPI.2016.042
 *   http://sibgrapi.sid.inpe.br/col/sid.inpe.br/sibgrapi/2016/07.22.13.56/doc/PaperSIBGRAPI-2016_vFinal.pdf
 *
 *******************************************************************************************************************
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

#include <iostream>
#include <memkind_allocator.h>
#include <memkind.h>
#include <asm/unistd.h>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <linux/perf_event.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "Cprr.hpp"


/* Constructor */
Cprr::Cprr() {

}

void Cprr::loadParameters() {
    Exec exec = Exec::getInstance();
    exec.getConfigVariable(l, "PARAM_CPRR_L");
    exec.getConfigVariable(k, "PARAM_CPRR_K");
    exec.getConfigVariable(t, "PARAM_CPRR_T");
}

void Cprr::checkParameters() {
    if (l > n) {
        std::cout << "L can't be greater than N" << std::endl;
        std::cout << "Aborting..." << std::endl;
        exit(1);
    }
    if (k > l) {
        std::cout << "K can't be greater than L" << std::endl;
        std::cout << "Aborting..." << std::endl;
        exit(1);
    }
}

void Cprr::initDataStructuresUdl() {
    std::cout << "Initializing data structures..." << std::endl;

#ifdef USE_OPTANE_OTHERS
    std::cout << "Usando Optane para estruturas auxiliares\n";
#endif
#ifdef USE_OPTANE_MATRIX
    std::cout << "Usando Optane para matriz principal\n";
#endif

    /**** CODIGO EH O MESMO ****
    rkLists.resize(n*l); //Elisa: inicializa lista rankeada geral
    initSparseMatrix(matrix); //Elisa: inicializa matriz esparsa com tamanho do dataset, passa como
    //parâmetro da função o ponteiro para matrix das estruturas gerais do UDL

    imgRef.resize(n*l); //Elisa: inicializa um vetor img.Ref unsigned int
    posRef.resize(n*l);
    nRef.resize(n);

    // #elif defined(USE_OPTANE_MATRIX)
    // std::cout << "Usando Optane para matriz\n";

    // rkLists.resize(n*l); //Elisa: inicializa lista rankeada geral
    // initSparseMatrix_LibmemKind(matrix); //Elisa: inicializa matriz esparsa com tamanho do dataset, passa como
    // //parâmetro da função o ponteiro para matrix das estruturas gerais do UDL

    // imgRef.resize(n*l); //Elisa: inicializa um vetor img.Ref unsigned int
    // posRef.resize(n*l);
    // nRef.resize(n);

    ****/

        //Elisa: n é o tamanho do dataset e l a profundidade da lista ligada
    rkLists.resize(n*l); //Elisa: inicializa lista rankeada geral
    initSparseMatrix(matrix); //Elisa: inicializa matriz esparsa com tamanho do dataset, passa como
        //parâmetro da função o ponteiro para matrix das estruturas gerais do UDL

    imgRef.resize(n*l); //Elisa: inicializa um vetor img.Ref unsigned int
    posRef.resize(n*l);
    nRef.resize(n);

    std::cout << "Tamanhos Estruturas de dados" << std::endl;
    std::cout << imgRef.size()*sizeof(imgRef[0]) << '\n';
    std::cout << posRef.size()*sizeof(posRef[0]) << '\n';
    std::cout << nRef.size()*sizeof(nRef[0]) << '\n';
    std::cout << n*n*sizeof(matrix[0]) << '\n';
    std::cout << rkLists.size()*sizeof(rkLists[0])<< '\n';
    //#endif
    
    std::cout << "Initialized successfully!" << std::endl;
}

void Cprr::initDataStructuresFusion() {
    std::cout << "Initializing data structures..." << std::endl;

    initDataStructuresUdl(); //init the main structures

    //structures used for aggregation (fusion) tasks
    initSparseMatrix(matrixAgg);//Elisa: inicializa matriz esparsa com tamanho do dataset, passa como
    //parâmetro da função o ponteiro para matrix de estruturtas agregadas
    rkListsAgg.resize(n);  //make sure it's empty //Elisa: inicializa lista rankeada agregada

    std::cout << "Initialized successfully!" << std::endl;
}

void Cprr::prepareInput() {
    if (inputFileFormat == "MATRIX") {
        if (inputMatrixType == "DIST") {
            genRksFromDistMatrix();
        } else { //SIM
            genRksFromSimMatrix();
        }
        initSparseMatrix(matrix);
    }
}

void Cprr::prepareOutput() {
    if (outputFileFormat == "MATRIX") {
        if (outputMatrixType == "DIST") {
            genDistMatrixFromRks();
        } else { //SIM
            genSimMatrixFromRks();
        }
    }
}

void Cprr::runUdlMethod() {
    std::cout << std::endl << "  -> Noniterative Steps" << std::endl;
    execFillPosMatrix();
    execSortRankedListsZero();

    int iteration = 1;
    while (iteration <= t) {
        std::cout << std::endl << "  -> Iteration " << iteration << std::endl;
        execCartProd();
        execReverseCartProd();
        execCleanRef();
        if (iteration != t) {
            execSortRankedListsZero();
        } else {
            execSortRankedLists();
        }
        iteration++;
    }
}

void Cprr::runFusionMethod() {
    for (std::string const& file : fusionFiles) {
        //prepare the ranked lists to run
        gettimeofday(&startTimeToDecrement, NULL);
            readInputFile(file);
            prepareInput();
        totalTimeToDecrement = Time::addTime(startTimeToDecrement, totalTimeToDecrement);

        //all the images that appear in the main ranked list are stored in a secondary one (union of ranked lists)
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < l; j++) {
                int img = rkLists[l*i + j];
                if (!search(img, rkListsAgg[i])) {
                    rkListsAgg[i].push_back(img);
                }
            }
        }

        //execute the main method
        runUdlMethod();

        //store values in a secondary matrix
        for (long int i = 0; i < n*n; i++) {
            matrixAgg[i] += matrix[i];
        }

        //reinitialize the main matrix for the next iteration
        initSparseMatrix(matrix);
    }

    std::cout << std::endl << " Fusion Steps: " << std::endl;

    delete [] matrix;   //the main values are not necessary anymore
    matrix = matrixAgg; //the current matrix is now the one that has the accumulated values

    //execute a sorting to prepare for the final iterations
    execSortRankedListsAgg(rkListsAgg);

    //copy the content between ranked lists
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < l; j++) {
            rkLists[l*i + j] = rkListsAgg[i][j];
        }
    }
    rkListsAgg.clear(); //these values are not necessary anymore

    //final processing
    std::cout << std::endl << " Final Processing: " << std::endl;
    int steps = 2; //this is a fixed value for the final iterations
    int iteration = 1;
    while (iteration <= steps) {
        std::cout << std::endl << "  -> Step " << iteration << std::endl;
        execCartProd();
        execReverseCartProd();
        execCleanRef();
        if (iteration != steps) {
            execSortRankedListsZero();
        } else {
            execSortRankedLists();
        }
        iteration++;
    }
}

void Cprr::execFillPosMatrix() {
    std::cout << "\n\t Fill Distance Matrix with Positions ... \n";

    ////Elisa: print(tempo inicio)
    printf ("\nInicio da marcacao de tempo FillPosmatrix\n");
    gettimeofday(&startTimeElapsedFillPosmatrix, NULL);
    //bloco código adicionado 15ago2026

    //Salvar para imprimir no log (tempo inicio)
    timeval startFillPosMatrixLog;
    gettimeofday(&startFillPosMatrixLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelFillPosMatrix(i);
    }
    
    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));
    
    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);

    //Salvar para imprimir no log
    perfCountersByStage["FillPosMatrix_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["FillPosMatrix_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["FillPosMatrix_page-faults"].push_back(pageFaults);

    perfRatesByStage["FillPosMatrix_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026



    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(startFillPosMatrixLog, 0);
    timesByStage["startFillPosMatrixLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsedFillPosmatrix = Time::addTime(startTimeElapsedFillPosmatrix, 0);
    printf ("Fim da marcacao de tempo FillPosmatrix\n");
    std::cout << "  -> Tempo gasto em FillPosMatrix: " << totalTimeElapsedFillPosmatrix << " s\n";
    //bloco código adicionado 15ago2026
}

void Cprr::execSortRankedLists() {
    std::cout << "\n\t Sort Ranked Lists ... \n";

    //print da Elisa (tempo inicio)
    printf ("\nInicio da marcacao de tempo SortRankedLists\n");
    gettimeofday(&startTimeElapsed_SortRankedLists, NULL);
    //bloco código adicionado 15ago2026

    //Salvar para imprimir no log (tempo inicio)
    timeval startSortRankedListLog;
    gettimeofday(&startSortRankedListLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelSortRankedLists(i);
    }

    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));

    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);
    
    //Salvar para imprimir no log
    perfCountersByStage["SortRankedList_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["SortRankedList_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["SortRankedList_page-faults"].push_back(pageFaults);

    perfRatesByStage["SortRankedList_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026

    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(startSortRankedListLog, 0);
    timesByStage["startSortRankedListLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsed_SortRankedLists = Time::addTime(startTimeElapsed_SortRankedLists, 0);
    printf ("Fim da marcacao de tempo SortRankedLists\n");
    std::cout << "  -> Tempo gasto em SortRankedLists: " << totalTimeElapsed_SortRankedLists << " s\n";
    //bloco código adicionado 15ago2026    

}

void Cprr::execSortRankedListsZero() {
    std::cout << "\n\t Sort Ranked Lists (Zero) ... \n";

    //Elisa: print(tempo inicio)
    printf ("\nInicio da marcacao de tempo SortRankedListsZero\n");
    gettimeofday(&startTimeElapsedSortRankedListsZero, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Salvar para imprimir no log (tempo inicio)
    timeval startSortRankedListsZeroLog;
    gettimeofday(&startSortRankedListsZeroLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelSortRankedListsZero(i);
    }

    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));

    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);
    
    //Salvar para imprimir no log
    perfCountersByStage["SortRankedListsZero_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["SortRankedListsZero_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["SortRankedListsZero_page-faults"].push_back(pageFaults);

    perfRatesByStage["SortRankedListsZero_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026

    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(startSortRankedListsZeroLog, 0);
    timesByStage["startSortRankedListsZeroLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsedSortRankedListsZero = Time::addTime(startTimeElapsedSortRankedListsZero, 0);
    printf ("Fim da marcacao de tempo SortRankedListsZero\n");
    std::cout << "  -> Tempo gasto em SortRankedListsZero: " << totalTimeElapsedSortRankedListsZero << " s\n";
    //bloco código adicionado 15ago2026
}

void Cprr::execSortRankedListsAgg(std::vector<std::vector<int>>& rk) {
    std::cout << "\n\t Sort Ranked Lists (Fusion) ... \n";
    for (int i = 0; i < n; i++) {
        kernelSortRankedListsAgg(i, rk);
    }
}

void Cprr::execCartProd() {
    std::cout << "\n\t Cartesian Product ... \n";

    //print da Elisa (tempo inicio)
    printf ("\nInicio da marcacao de tempo CartProd\n");
    gettimeofday(&startTimeElapsedCartProd, NULL);
    //bloco código adicionado 15ago2026

    //Salvar para imprimir no log (tempo inicio)
    timeval CartProdLog;
    gettimeofday(&CartProdLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelCartProd(i);
    }

    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));

    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);
    
    //Salvar para imprimir no log
    perfCountersByStage["CartProd_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["CartProd_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["CartProd_page-faults"].push_back(pageFaults);

    perfRatesByStage["CartProd_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026

    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(CartProdLog, 0);
    timesByStage["CartProdLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsedCartProd = Time::addTime(startTimeElapsedCartProd, 0);
    printf ("Fim da marcacao de tempo CartProd\n");
    std::cout << "  -> Tempo gasto em CartProd: " << totalTimeElapsedCartProd << " s\n";
    //bloco código adicionado 15ago2026
}

void Cprr::execReverseCartProd() {
    std::cout << "\n\t Reverse Cartesian Product ... \n";

    //print da Elisa (tempo inicio)
    printf ("\nInicio da marcacao de tempo ReverseCartProd\n");
    gettimeofday(&startTimeElapsed_ReverseCartProd, NULL);
    //bloco código adicionado 15ago2026

    //Salvar para imprimir no log (tempo inicio)
    timeval ReverseCartProdLog;
    gettimeofday(&ReverseCartProdLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelReverseCartProd(i);
    }

    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));

    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);
    
    //Salvar para imprimir no log
    perfCountersByStage["ReverseCartProd_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["ReverseCartProd_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["ReverseCartProd_page-faults"].push_back(pageFaults);

    perfRatesByStage["ReverseCartProd_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026

    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(ReverseCartProdLog, 0);
    timesByStage["ReverseCartProdLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsed_ReverseCartProd = Time::addTime(startTimeElapsed_ReverseCartProd, 0);
    printf ("Fim da marcacao de tempo ReverseCartProd\n");
    std::cout << "  -> Tempo gasto em ReverseCartProd: " << totalTimeElapsed_ReverseCartProd << " s\n";
    //bloco código adicionado 15ago2026
}

void Cprr::execCleanRef() {
    std::cout << "\n\t Clean Reference List ... \n";

    //print da Elisa (tempo inicio)
    printf ("\nInicio da marcacao de tempo CleanRef\n");
    gettimeofday(&startTimeElapsed_CleanRef, NULL);
    //bloco código adicionado 15ago2026

    //Salvar para imprimir no log (tempo inicio)
    timeval CleanRefLog;
    gettimeofday(&CleanRefLog, NULL);
    //bloco código adicionado 15ago2026

    //Elisa: Inicio medicao eventos com Perf
    // =====================================================
    // 1. Configuração do evento LLC-loads
    // =====================================================

    perf_event_attr attrLlcLoads{};

    attrLlcLoads.type = PERF_TYPE_HW_CACHE;
    attrLlcLoads.size = sizeof(attrLlcLoads);

    attrLlcLoads.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16);

    attrLlcLoads.disabled = 1;
    attrLlcLoads.exclude_kernel = 1;
    attrLlcLoads.exclude_hv = 1;

    int fdLlcLoads = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoads,
        0,      // thread atual
        -1,     // qualquer CPU
        -1,     // líder do grupo
        0
    ));

    // =====================================================
    // 2. Configuração do evento LLC-load-misses
    // =====================================================

    perf_event_attr attrLlcLoadMisses{};

    attrLlcLoadMisses.type = PERF_TYPE_HW_CACHE;
    attrLlcLoadMisses.size = sizeof(attrLlcLoadMisses);

    attrLlcLoadMisses.config =
        PERF_COUNT_HW_CACHE_LL |
        (PERF_COUNT_HW_CACHE_OP_READ << 8) |
        (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);

    attrLlcLoadMisses.disabled = 1;
    attrLlcLoadMisses.exclude_kernel = 1;
    attrLlcLoadMisses.exclude_hv = 1;

    int fdLlcLoadMisses = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrLlcLoadMisses,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // =====================================================
    // 3. Configuração do evento page-faults
    // =====================================================

    perf_event_attr attrPageFaults{};

    attrPageFaults.type = PERF_TYPE_SOFTWARE;
    attrPageFaults.size = sizeof(attrPageFaults);
    attrPageFaults.config = PERF_COUNT_SW_PAGE_FAULTS;

    attrPageFaults.disabled = 1;
    attrPageFaults.exclude_kernel = 1;
    attrPageFaults.exclude_hv = 1;

    int fdPageFaults = static_cast<int>(syscall(
        __NR_perf_event_open,
        &attrPageFaults,
        0,              // thread atual
        -1,             // qualquer CPU
        fdLlcLoads,     // pertence ao grupo de fdLlcLoads
        0
    ));

    // Zera e inicia todos os contadores do grupo
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_RESET,
        PERF_IOC_FLAG_GROUP
    );

    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_ENABLE,
        PERF_IOC_FLAG_GROUP
    );
    //Elisa: bloco código adicionado 20set2026

    for (int i = 0; i < n; i++) {
        kernelCleanRef(i);
    }

    //Elisa: Fim medicao eventos com Perf
     // Para todos os contadores
    ioctl(
        fdLlcLoads,
        PERF_EVENT_IOC_DISABLE,
        PERF_IOC_FLAG_GROUP
    );

    // Lê os valores absolutos
    std::uint64_t llcLoads = 0;
    std::uint64_t llcLoadMisses = 0;
    std::uint64_t pageFaults = 0;

    ssize_t bytesLlcLoads = read(fdLlcLoads, &llcLoads, sizeof(llcLoads));

    ssize_t bytesLlcLoadMisses = read(fdLlcLoadMisses, &llcLoadMisses, sizeof(llcLoadMisses));

    ssize_t bytesPageFaults = read(fdPageFaults, &pageFaults, sizeof(pageFaults));

    // Calcula a porcentagem de LLC-load-misses
    double llcMissRate = 0.0;

    if (llcLoads != 0) {
        llcMissRate =
            100.0 * static_cast<double>(llcLoadMisses)
            / static_cast<double>(llcLoads);
    }

    std::cout << "LLC-loads: "
              << llcLoads << '\n';

    std::cout << "LLC-load-misses: "
              << llcLoadMisses << '\n';

    std::cout << std::fixed << std::setprecision(2);

    std::cout << "LLC-load-miss rate: "
              << llcMissRate << "%\n";

    std::cout << "Page faults: "
              << pageFaults << '\n';

    // Libera os contadores
    close(fdPageFaults);
    close(fdLlcLoadMisses);
    close(fdLlcLoads);
    
    //Salvar para imprimir no log
    perfCountersByStage["CleanRef_LLC-loads"].push_back(llcLoads);

    perfCountersByStage["CleanRef_LLC-load-misses"].push_back(llcLoadMisses);

    perfCountersByStage["CleanRef_page-faults"].push_back(pageFaults);

    perfRatesByStage["CleanRef_LLC-miss-rate"].push_back(llcMissRate);
    //Elisa: bloco código adicionado 20set2026

    //Salvar para imprimir no log (tempo fim)
    float duration = Time::addTime(CleanRefLog, 0);
    timesByStage["CleanRefLog"].push_back(duration);
    //bloco código adicionado 15ago2026

    //print da Elisa (tempo fim)
    totalTimeElapsed_CleanRef = Time::addTime(startTimeElapsed_CleanRef, 0);
    printf ("Fim da marcacao de tempo CleanRef\n");
    std::cout << "  -> Tempo gasto em CleanRef: " << totalTimeElapsed_CleanRef << " s\n";
    //bloco código adicionado 15ago2026
}

void Cprr::kernelFillPosMatrix(int rk) {
    long int lrk = l*rk;
    long int nrk = n*rk;
    long int img;

    for (int pos = 0; pos < l; pos++) {
        img = rkLists[lrk + pos];
        matrix[nrk + img] += l - pos;
        matrix[n * img + rk] += l - pos;
    }
}

void Cprr::kernelSortRankedLists(int rk) {
    long int LcurRL = l*rk;
    long int cNcurRL = n*rk;
    float a[l];

    long int index;
    for (int j = 0; j < l; j++) {
        index = cNcurRL + rkLists[LcurRL + j];
        a[j] = matrix[index];
    }

    //---------------------- INSERTION SORT --------------------------
    int i, j, keyR;
    float keyA;

    for (j = 1; j < l; j++) {
        keyA = a[j];
        keyR = rkLists[LcurRL + j];
        i = j - 1;
        while (i >= 0 && a[i] < keyA) {
            a[i + 1] = a[i];
            rkLists[LcurRL + i + 1] = rkLists[LcurRL + i];
            i--;
        }
        a[i + 1] = keyA;
        rkLists[LcurRL + i + 1] = keyR;
    }
    //----------------------------------------------------------------

    //Setting query image at first position
    i = 0;
    while ((rkLists[LcurRL + i] != rk)&&(i < l)) {
        i++;
    }
    if (i > 0) {
        int aux = rkLists[LcurRL + 0];
        rkLists[LcurRL + 0] = rkLists[LcurRL + i];
        rkLists[LcurRL + i] = aux;

        float auxA = a[0];
        a[0] = a[i];
        a[i] = auxA;
    }
}

void Cprr::kernelSortRankedListsZero(int rk) {
    long int LcurRL = l*rk;
    long int cNcurRL = n*rk;
    float a[l];

    long int index;
    for (int j = 0; j < l; j++) {
        index = cNcurRL + rkLists[LcurRL + j];
        a[j] = matrix[index];
        matrix[index] = 0;
    }

    //---------------------- INSERTION SORT --------------------------
    int i, j, keyR;
    float keyA;

    for (j = 1; j < l; j++) {
        keyA = a[j];
        keyR = rkLists[LcurRL + j];
        i = j - 1;
        while (i >= 0 && a[i] < keyA) {
            a[i + 1] = a[i];
            rkLists[LcurRL + i + 1] = rkLists[LcurRL + i];
            i--;
        }
        a[i + 1] = keyA;
        rkLists[LcurRL + i + 1] = keyR;
    }
    //----------------------------------------------------------------

    //Setting query image at first position
    i = 0;
    while ((rkLists[LcurRL + i] != rk)&&(i < l)) {
        i++;
    }
    if (i > 0) {
        int aux = rkLists[LcurRL + 0];
        rkLists[LcurRL + 0] = rkLists[LcurRL + i];
        rkLists[LcurRL + i] = aux;

        float auxA = a[0];
        a[0] = a[i];
        a[i] = auxA;
    }
}

void Cprr::kernelSortRankedListsAgg(int curRL, std::vector<std::vector<int>>& rk) {
    int LcurRL = rk[curRL].size();
    int cNcurRL = n*curRL;
    float a[rk[curRL].size()];

    for (int j = 0; j < rk[curRL].size(); j++) {
        a[j] = matrix[cNcurRL + rk[curRL][j]];
    }

    //---------------------- INSERTION SORT --------------------------
    int i, j, keyR;
    float keyA;

    for (j = 1; j < rk[curRL].size(); j++) {
        keyA = a[j];
        keyR = rk[curRL][j];
        i = j - 1;
        while (i >= 0 && a[i] < keyA) {
            a[i + 1] = a[i];
            rk[curRL][i + 1] = rk[curRL][i];
            i--;
        }
        a[i + 1] = keyA;
        rk[curRL][i + 1] = keyR;
    }
    //----------------------------------------------------------------

    //Setting query image at first position
    i = 0;
    while ((rk[curRL][i] != curRL)&&(i < rk[curRL].size())) {
        i++;
    }
    if (i > 0) {
        int aux = rk[curRL][0];
        rk[curRL][0] = rk[curRL][i];
        rk[curRL][i] = aux;

        float auxA = a[0];
        a[0] = a[i];
        a[i] = auxA;
    }
}

void Cprr::kernelCartProd(int rk) {
    int set[k];
    set[0] = rk;

    long int Li = l*rk;
    long int imgNei, index;

    for (long int j = 1; j < k; j++) {
        imgNei = rkLists[Li + j];
        set[j] = imgNei;
        index = l * imgNei + nRef[imgNei];
        imgRef[index] = rk;
        posRef[index] = k - j;
        nRef[imgNei]++;
    }

    long int idx, idy, Nidx;
    double score;

    for (long int j = 0; j < k; j++) {
        idx = set[j];
        Nidx = n*idx;
        for (long int l = 0; l < k; l++) {
            idy = set[l];
            score = ((k - (j + 1) + 1)*(k - (l + 1) + 1));
            matrix[Nidx + idy] += score;
            matrix[n * idy + idx] += score;
        }
    }

}

void Cprr::kernelReverseCartProd(int rk) {
    long int size = nRef[rk];
    long int Li = l*rk;
    long int idx, idy, Nidx, index, index2;
    double score;

    for (long int j = 0; j < size; j++) {
        index = Li + j;
        idx = imgRef[index];
        Nidx = n*idx;
        for (long int l = 0; l < size; l++) {
            index2 = Li + l;
            idy = imgRef[index2];
            score = (posRef[index])*(posRef[index2]);
            matrix[Nidx + idy] += score;
            matrix[n * idy + idx] += score;
        }
    }
}

void Cprr::kernelCleanRef(int rk) {
    nRef[rk] = 0;
}

bool Cprr::search(int img, const std::vector<int>& rk) {
    for (int i = 0; i < rk.size(); i++) {
        if (img == rk[i]) {
            return true;
        }
    }
    return false;
}
