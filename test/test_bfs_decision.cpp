//Own includes
#include "dynamic/degreeDistInfo.hpp"
#include <algorithm>
#include <random>

//External includes
#include "mxx/comm.hpp"
#include <gtest/gtest.h>

/**
 * @brief       coloring of undirected graph with 3 components
 * @details     builds a small test graph with three components, 
 *              test if program returns 3 as the component count
 */
TEST(connColoring, smallUndirected) {

  mxx::comm c = mxx::comm();

  //Declare a edgeList vector to save edges
  std::vector< std::pair<int64_t, int64_t> > edgeList;

  using nodeIdType = int64_t;

  //Start adding the edges
  if (c.rank() == 0) {

    //First component (2,3,4,11)
    //2--11
    edgeList.emplace_back(2,11);
    edgeList.emplace_back(11,2);

    //2--3
    edgeList.emplace_back(2,3);
    edgeList.emplace_back(3,2);

    //2--4
    edgeList.emplace_back(2,4);
    edgeList.emplace_back(4,2);

    //3--4
    edgeList.emplace_back(3,4);
    edgeList.emplace_back(4,3);

    //Second component (5,6,8,10)
    //5--6
    edgeList.emplace_back(5,6);
    edgeList.emplace_back(6,5);

    //5--8
    edgeList.emplace_back(5,8);
    edgeList.emplace_back(8,5);

    //6--10
    edgeList.emplace_back(6,10);
    edgeList.emplace_back(10,6);

    //6--8
    edgeList.emplace_back(6,8);
    edgeList.emplace_back(8,6);

    //Third component (50,51,52)
    for(int i = 50; i < 52 ; i++)
    {
      edgeList.emplace_back(i, i+1);
      edgeList.emplace_back(i+1, i);
    }
  }

  // std::random_shuffle(edgeList.begin(), edgeList.end());
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(edgeList.begin(), edgeList.end(), g);

  //Since we have a graph of small size, use less processes
  c.with_subset(c.rank() < 4, [&](const mxx::comm &comm) {
    conn::dynamic::runBFSDecision(edgeList, comm);
  });
}
