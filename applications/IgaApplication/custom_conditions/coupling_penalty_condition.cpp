//    |  /           |
//    ' /   __| _` | __|  _ \   __|
//    . \  |   (   | |   (   |\__ `
//   _|\_\_|  \__,_|\__|\___/ ____/
//                   Multi-Physics
//
//  License:         BSD License
//                   Kratos default license: kratos/license.txt
//
//  Main authors:    Tobias Tescheamacher
//                   Michael Breitenberger
//                   Riccardo Rossi
//

// System includes

// External includes

// Project includes
#include "custom_conditions/coupling_penalty_condition.h"

namespace Kratos
{
    void CouplingPenaltyCondition::CalculateAll(
        MatrixType& rLeftHandSideMatrix,
        VectorType& rRightHandSideVector,
        const ProcessInfo& rCurrentProcessInfo,
        const bool CalculateStiffnessMatrixFlag,
        const bool CalculateResidualVectorFlag)
    {
        KRATOS_TRY
        const double penalty = GetProperties()[PENALTY_FACTOR];

        const auto& r_geometry_master = GetGeometry().GetGeometryPart(0);
        const auto& r_geometry_slave = GetGeometry().GetGeometryPart(1);

        // Size definitions
        const SizeType number_of_nodes_master = r_geometry_master.size();
        const SizeType number_of_nodes_slave = r_geometry_slave.size();

        const SizeType mat_size = 3 * (number_of_nodes_master + number_of_nodes_slave);

        // Memory allocation
        if (CalculateStiffnessMatrixFlag) {
            if (rLeftHandSideMatrix.size1() != mat_size) {
                rLeftHandSideMatrix.resize(mat_size, mat_size, false);
            }
            noalias(rLeftHandSideMatrix) = ZeroMatrix(mat_size, mat_size);
        }
        if (CalculateResidualVectorFlag) {
            if (rRightHandSideVector.size() != mat_size) {
                rRightHandSideVector.resize(mat_size, false);
            }
            rRightHandSideVector = ZeroVector(mat_size);
        }

        // Integration
        const GeometryType::IntegrationPointsArrayType& integration_points = r_geometry_master.IntegrationPoints();

        // Determine the integration: conservative -> initial; non-conservative -> current
        Vector determinant_jacobian_vector(integration_points.size());
        const bool integrate_conservative = GetProperties().Has(INTEGRATE_CONSERVATIVE)
            ? GetProperties()[INTEGRATE_CONSERVATIVE]
            : false;
        if (integrate_conservative) {
            DeterminantOfJacobianInitial(r_geometry_master, determinant_jacobian_vector);
        }
        else {
            r_geometry_master.DeterminantOfJacobian(determinant_jacobian_vector);
        }

        for (IndexType point_number = 0; point_number < integration_points.size(); point_number++)
        {
            Matrix N_master = r_geometry_master.ShapeFunctionsValues();
            Matrix N_slave = r_geometry_slave.ShapeFunctionsValues();

            //FOR DISPLACEMENTS
            Matrix H = ZeroMatrix(3, mat_size);
            for (IndexType i = 0; i < number_of_nodes_master; ++i)
            {
                IndexType index = 3 * i;
                if (Is(IgaFlags::FIX_DISPLACEMENT_X))
                    H(0, index) = N_master(point_number, i);
                if (Is(IgaFlags::FIX_DISPLACEMENT_Y))
                    H(1, index + 1) = N_master(point_number, i);
                if (Is(IgaFlags::FIX_DISPLACEMENT_Z))
                    H(2, index + 2) = N_master(point_number, i);
            }

            for (IndexType i = 0; i < number_of_nodes_slave; ++i)
            {
                IndexType index = 3 * (i + number_of_nodes_master);
                if (Is(IgaFlags::FIX_DISPLACEMENT_X))
                    H(0, index) = -N_slave(point_number, i);
                if (Is(IgaFlags::FIX_DISPLACEMENT_Y))
                    H(1, index + 1) = -N_slave(point_number, i);
                if (Is(IgaFlags::FIX_DISPLACEMENT_Z))
                    H(2, index + 2) = -N_slave(point_number, i);
            }

            // Differential area
            const double penalty_integration = penalty * integration_points[point_number].Weight() * determinant_jacobian_vector[point_number];

            // Assembly
            if (CalculateStiffnessMatrixFlag) {
                noalias(rLeftHandSideMatrix) += prod(trans(H), H) * penalty_integration;
            }
            if (CalculateResidualVectorFlag) {

                Vector u(mat_size);
                for (IndexType i = 0; i < number_of_nodes_master; ++i)
                {
                    const array_1d<double, 3> disp = r_geometry_master[i].FastGetSolutionStepValue(DISPLACEMENT);
                    IndexType index = 3 * i;
                    u[index]     = disp[0];
                    u[index + 1] = disp[1];
                    u[index + 2] = disp[2];
                }
                for (IndexType i = 0; i < number_of_nodes_slave; ++i)
                {
                    const array_1d<double, 3> disp = r_geometry_slave[i].FastGetSolutionStepValue(DISPLACEMENT);
                    IndexType index = 3 * (i + number_of_nodes_master);
                    u[index]     = disp[0];
                    u[index + 1] = disp[1];
                    u[index + 2] = disp[2];
                }

                noalias(rRightHandSideVector) -= prod(prod(trans(H), H), u) * penalty_integration;
            }
        }

        KRATOS_CATCH("")
    }

    void CouplingPenaltyCondition::DeterminantOfJacobianInitial(
        const GeometryType& rGeometry,
        Vector& rDeterminantOfJacobian)
    {
        const IndexType nb_integration_points = rGeometry.IntegrationPointsNumber();
        if (rDeterminantOfJacobian.size() != nb_integration_points) {
            rDeterminantOfJacobian.resize(nb_integration_points, false);
        }

        const SizeType working_space_dimension = rGeometry.WorkingSpaceDimension();
        const SizeType local_space_dimension = rGeometry.LocalSpaceDimension();
        const SizeType nb_nodes = rGeometry.PointsNumber();

        Matrix J = ZeroMatrix(working_space_dimension, local_space_dimension);
        for (IndexType pnt = 0; pnt < nb_integration_points; pnt++)
        {
            const Matrix& r_DN_De = rGeometry.ShapeFunctionsLocalGradients()[pnt];
            J.clear();
            for (IndexType i = 0; i < nb_nodes; ++i) {
                const array_1d<double, 3>& r_coordinates = rGeometry[i].GetInitialPosition();
                for (IndexType k = 0; k < working_space_dimension; ++k) {
                    const double value = r_coordinates[k];
                    for (IndexType m = 0; m < local_space_dimension; ++m) {
                        J(k, m) += value * r_DN_De(i, m);
                    }
                }
            }

            //Compute the tangent and  the normal to the boundary vector
            array_1d<double, 3> local_tangent;
            GetGeometry().GetGeometryPart(0).Calculate(LOCAL_TANGENT, local_tangent);

            array_1d<double, 3> a_1 = column(J, 0);
            array_1d<double, 3> a_2 = column(J, 1);

            rDeterminantOfJacobian[pnt] = norm_2(a_1 * local_tangent[0] + a_2 * local_tangent[1]);
        }
    }

    int CouplingPenaltyCondition::Check(const ProcessInfo& rCurrentProcessInfo) const
    {
        KRATOS_ERROR_IF_NOT(GetProperties().Has(PENALTY_FACTOR))
            << "No penalty factor (PENALTY_FACTOR) defined in property of SupportPenaltyCondition" << std::endl;
        return 0;
    }

    void CouplingPenaltyCondition::EquationIdVector(
        EquationIdVectorType& rResult,
        const ProcessInfo& rCurrentProcessInfo) const
    {
        KRATOS_TRY;

        const auto& r_geometry_master = GetGeometry().GetGeometryPart(0);
        const auto& r_geometry_slave = GetGeometry().GetGeometryPart(1);

        const SizeType number_of_nodes_master = r_geometry_master.size();
        const SizeType number_of_nodes_slave = r_geometry_slave.size();

        if (rResult.size() != 3 * (number_of_nodes_master + number_of_nodes_slave))
            rResult.resize(3 * (number_of_nodes_master + number_of_nodes_slave), false);

        for (IndexType i = 0; i < number_of_nodes_master; ++i) {
            const IndexType index = i * 3;
            const auto& r_node = r_geometry_master[i];
            rResult[index]     = r_node.GetDof(DISPLACEMENT_X).EquationId();
            rResult[index + 1] = r_node.GetDof(DISPLACEMENT_Y).EquationId();
            rResult[index + 2] = r_node.GetDof(DISPLACEMENT_Z).EquationId();
        }

        for (IndexType i = 0; i < number_of_nodes_slave; ++i) {
            const IndexType index = 3 * (i + number_of_nodes_master);
            const auto& r_node = r_geometry_slave[i];
            rResult[index]     = r_node.GetDof(DISPLACEMENT_X).EquationId();
            rResult[index + 1] = r_node.GetDof(DISPLACEMENT_Y).EquationId();
            rResult[index + 2] = r_node.GetDof(DISPLACEMENT_Z).EquationId();
        }

        KRATOS_CATCH("")
    }

    void CouplingPenaltyCondition::GetDofList(
        DofsVectorType& rElementalDofList,
        const ProcessInfo& rCurrentProcessInfo) const
    {
        KRATOS_TRY;

        const auto r_geometry_master = GetGeometry().GetGeometryPart(0);
        const auto r_geometry_slave = GetGeometry().GetGeometryPart(1);

        const SizeType number_of_nodes_master = r_geometry_master.size();
        const SizeType number_of_nodes_slave = r_geometry_slave.size();

        rElementalDofList.resize(0);
        rElementalDofList.reserve(3 * (number_of_nodes_master + number_of_nodes_slave));

        for (IndexType i = 0; i < number_of_nodes_master; ++i) {
            const auto& r_node = r_geometry_master.GetPoint(i);
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_X));
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_Y));
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_Z));
        }

        for (IndexType i = 0; i < number_of_nodes_slave; ++i) {
            const auto& r_node = r_geometry_slave.GetPoint(i);
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_X));
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_Y));
            rElementalDofList.push_back(r_node.pGetDof(DISPLACEMENT_Z));
        }

        KRATOS_CATCH("")
    }
} // Namespace Kratos


