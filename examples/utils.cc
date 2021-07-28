#include "utils.h"
#include <random>
#include <eigen3/Eigen/Dense>
#include <algorithm>

std::vector<mobile_node_t> utils::get_ue(Vector2d roi_means, Vector2d roi_vars) {
    int n_ues = 5;
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<> d1{roi_means(0),roi_vars(0)};
    std::normal_distribution<> d2{roi_means(1),roi_vars(1)};

    std::vector<mobile_node_t> ue_nodes;

    for(int i=0; i<n_ues; i++) {
        Vector pose(d1(gen), d2(gen), 0); 
        mobile_node_t ue_node = {.position=pose, .id=i};
        // NS_LOG_INFO("STA Locations: "<< "id: "<<i << " pose: "<< pose );
        ue_nodes.push_back(ue_node);   
    }

    typedef std::function<bool(mobile_node_t, mobile_node_t)> Comparator;
    Comparator compFunctor =
            [](mobile_node_t elem1, mobile_node_t elem2) {
                return elem1.position.GetLength() < elem2.position.GetLength();
            };
    std::sort(ue_nodes.begin(), ue_nodes.end(), compFunctor);
    return ue_nodes;

}
