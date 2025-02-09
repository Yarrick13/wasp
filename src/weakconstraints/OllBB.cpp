/*
 *
 *  Copyright 2013 Mario Alviano, Carmine Dodaro, and Francesco Ricca.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "OllBB.h"

unsigned int
OllBB::run()
{   
    trace_msg( weakconstraints, 1, "Starting algorithm OLLBB" );        

    solver.turnOffSimplifications();
    initInUnsatCore();
    originalNumberOfVariables = solver.numberOfVariables();
    solver.sortOptimizationLiterals( level() );
    initHeuristicValues();
    
    unsigned int i = 0;
    while( true )
    {
        setAndUpdateHeuristicValues();
        unsigned int res;
        trace_msg( weakconstraints, 2, "Iteration " << i );
        if( i++ % 2 == 0 )
            res = oll();
        else
            res = bb();
        
        if( lb() == ub() || res == OPTIMUM_FOUND )
        {
            if( !strategyModelGuided->completedLevel() )
                return OPTIMUM_FOUND_STOP;
            return OPTIMUM_FOUND;
        }
        
        if( res == INCOHERENT )
        {
            if( numberOfModels == 0 )
                return INCOHERENT;
            else
            {
                if( !strategyModelGuided->completedLevel() )
                    return OPTIMUM_FOUND_STOP;
                return OPTIMUM_FOUND;
            }
        }
    }
}

unsigned int
OllBB::bb()
{
    trace_msg( weakconstraints, 3, "Starting BB" );
    solver.unrollToZero();
    assumptions.clear();
    addOptimizationLiteralInAssumptions();

    solver.setComputeUnsatCores( false );    
    unsigned int res = solver.solve();
    while( res == COHERENT )
    {
        numberOfModels++;
        uint64_t modelCost = solver.computeCostOfModel( level() );
        foundAnswerSet( modelCost );
        trace_msg( weakconstraints, 4, "Decision level of solver: " << solver.getCurrentDecisionLevel() );
        if( ub() == lb() || ub() == 0 || solver.getCurrentDecisionLevel() == 0 )
            break;
        
        trace_msg( weakconstraints, 4, "Updating bound of optimization aggregate. Model cost: " << ub() );        
        if( !strategyModelGuided->updateOptimizationAggregate( modelCost ) )
        {
            trace_msg( weakconstraints, 5, "Failed updating of optimization aggregate: return" );
            break;        
        }                
        
        trace_msg( weakconstraints, 4, "Calling solver..." );
        res = solver.solve();
    }

    if( res == INTERRUPTED )
        return res;    
    return OPTIMUM_FOUND;    
}

unsigned int
OllBB::oll()
{
    trace_msg( weakconstraints, 3, "Starting OLL" );
    solver.unrollToZero();        
    assumptions.clear();
    solver.setComputeUnsatCores( true );    
    computeAssumptions();    
    unsigned int res = solver.solve( assumptions );    
    while( res == INCOHERENT )
    {        
        if( !foundUnsat() )
            return INCOHERENT;
        
        assumptions.clear();
        computeAssumptions();
        
        res = solver.solve( assumptions );        
    }
    
    if( res == INTERRUPTED )
        return res;

    foundAnswerSet( solver.computeCostOfModel( level() ) );
    assert_msg( lb() == ub(), lb() << " != " << ub() );
    numberOfModels++;
    
    return OPTIMUM_FOUND;    
}            