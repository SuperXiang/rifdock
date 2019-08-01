// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://wsic_dockosettacommons.org. Questions about this casic_dock
// (c) addressed to University of Waprotocolsgton UW TechTransfer, email: license@u.washington.eprotocols


#include <riflib/task/AnyPointTask.hh>
#include <riflib/task/util.hh>

#include <riflib/types.hh>


#include <string>
#include <vector>



namespace devel {
namespace scheme {


shared_ptr<std::vector<SearchPoint>> 
AnyPointTask::return_search_points( shared_ptr<std::vector<SearchPointWithRots>> search_point_with_rotss, RifDockData & rdd, ProtocolData & pd ) {
    runtime_assert(false); 
}
shared_ptr<std::vector<SearchPoint>> 
AnyPointTask::return_search_points( shared_ptr<std::vector<RifDockResult>> rif_dock_results, RifDockData & rdd, ProtocolData & pd ) {
    runtime_assert(false); 
}


shared_ptr<std::vector<SearchPointWithRots>> 
AnyPointTask::return_search_point_with_rotss( shared_ptr<std::vector<SearchPoint>> search_points, RifDockData & rdd, ProtocolData & pd ) { 
    runtime_assert(false); 
}
shared_ptr<std::vector<SearchPointWithRots>> 
AnyPointTask::return_search_point_with_rotss( shared_ptr<std::vector<RifDockResult>> rif_dock_results, RifDockData & rdd, ProtocolData & pd ) { 
    runtime_assert(false); 
}


shared_ptr<std::vector<RifDockResult>> 
AnyPointTask::return_rif_dock_results( shared_ptr<std::vector<SearchPoint>> search_points, RifDockData & rdd, ProtocolData & pd ) { 
    runtime_assert(false); 
}
shared_ptr<std::vector<RifDockResult>> 
AnyPointTask::return_rif_dock_results( shared_ptr<std::vector<SearchPointWithRots>> search_point_with_rotss, RifDockData & rdd, ProtocolData & pd ) { 
    runtime_assert(false); 
}

// boilerplate code for classes derived from AnyPoint 

// shared_ptr<std::vector<SearchPointWithRots>> 
// AnyPointTask::return_search_point_with_rotss( 
//     shared_ptr<std::vector<SearchPointWithRots>> search_point_with_rotss, 
//     RifDockData & rdd, 
//     ProtocolData & pd ) { 
//     return return_any_points( search_point_with_rotss, rdd, pd );
// }
// shared_ptr<std::vector<RifDockResult>> 
// AnyPointTask::return_rif_dock_results( 
//     shared_ptr<std::vector<RifDockResult>> rif_dock_results, 
//     RifDockData & rdd, 
//     ProtocolData & pd ) { 
//     return return_any_points( rif_dock_results, rdd, pd );
// }
// shared_ptr<std::vector<SearchPoint>> 
// AnyPointTask::return_search_points( 
//     shared_ptr<std::vector<SearchPoint>> search_points, 
//     RifDockData & rdd, 
//     ProtocolData & pd ) {
//     return return_any_points( search_points, rdd, pd );
// }



}}
