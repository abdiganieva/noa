#pragma once

#include <noa/pms/trace.hh>

#include <gtest/gtest.h>

using namespace noa::pms::trace;

using HostMesh = Meshes::Mesh<Meshes::DefaultConfig<Meshes::Topologies::Tetrahedron>, Devices::Host>;
using HostMeshOpt = std::optional<HostMesh>;

HostMeshOpt load_tetrahedron_host_test_mesh();

template <class DeviceType>
inline void test_get_first_border_in_tetrahedron() {
    using DeviceMesh = Meshes::Mesh<Meshes::DefaultConfig<Meshes::Topologies::Tetrahedron>, DeviceType>;
    using PointDevice = typename DeviceMesh::PointType;

    HostMeshOpt mesh_opt = load_tetrahedron_host_test_mesh();
    TNL_ASSERT_TRUE((bool) mesh_opt, "mesh don't loaded");
    HostMesh host_mesh = *mesh_opt;
    DeviceMesh device_mesh = host_mesh;
    Pointers::DevicePointer<const DeviceMesh> mesh_device_pointer(device_mesh);
    const DeviceMesh *mesh_pointer = &mesh_device_pointer.template getData<typename DeviceMesh::DeviceType>();

    auto check_tetrahedron_kernel = [=] __cuda_callable__ (int) mutable {
        int i = 0;
        int expected[] = {1, 0, 11, 12, 14, 8, 34, 36};
        int index = 1;
        PointDevice direction = PointDevice(0, 0, 1);
        direction /= sqrt(dot(direction, direction));
        PointDevice origin = PointDevice(0.1, 0.11, 0.1);
        while (true) {
            TNL_ASSERT_TRUE(i < 8, "incorrect index");
            TNL_ASSERT_EQ(expected[i], index, "wrong tetrahedron");

            auto result = Tracer<DeviceType>::get_first_border_in_tetrahedron(
                    mesh_pointer, index, origin, direction, 1e-7);

            TNL_ASSERT_TRUE(result.is_intersection_with_triangle, "incorrect result");

            origin += direction * result.distance;
            auto next_tetrahedron = Tracer<DeviceType>::get_next_tetrahedron(
                mesh_pointer, result, origin, direction, 1e-12);

            if (!next_tetrahedron) {
                break;
            }

            origin += result.distance * 1e-12;
            index = *next_tetrahedron;
            i++;
        }
        TNL_ASSERT_EQ(i, 7, "incorrect final index");
    };
    Algorithms::ParallelFor<DeviceType>::exec(0, 1, check_tetrahedron_kernel);

    auto check_incorrect_ray_kernel = [=] __cuda_callable__ (int) mutable {
        PointDevice direction = PointDevice(1, 1, 1);
        direction /= sqrt(dot(direction, direction));
        PointDevice origin = PointDevice(0.45, 0.45, 0.45);
        auto result = Tracer<DeviceType>::get_first_border_in_tetrahedron(
            mesh_pointer, 0, origin, direction, 1e-7);

        TNL_ASSERT_FALSE((bool)result.is_intersection_with_triangle, "incorrect result with incorrect ray");
    };
    Algorithms::ParallelFor<DeviceType>::exec(0, 1, check_incorrect_ray_kernel);

    auto check_back_corner_ray_kernel = [=] __cuda_callable__ (int) mutable {
        PointDevice direction = PointDevice(1, 1, 1);
        direction /= sqrt(dot(direction, direction));
        PointDevice origin = PointDevice(0.1, 0.1, 0.1);
        int index = 1;
        int expected[] = {1, 0, 12, 16, 38, 35, 44, 47};
        int i = 0;
        while (true) {
            TNL_ASSERT_TRUE(i < 8, "incorrect index in test with corners");
            TNL_ASSERT_EQ(expected[i], index, "wrong tetrahedron in test with corners");

            auto result = Tracer<DeviceType>::get_first_border_in_tetrahedron(
                    mesh_pointer, index, origin, direction, 1e-7);

            origin += direction * result.distance;
            auto next_tetrahedron = Tracer<DeviceType>::get_next_tetrahedron(
                    mesh_pointer, result, origin, direction, 1e-12);

            if (!next_tetrahedron) {
                break;
            }

            origin += direction * 1e-12;
            index = *next_tetrahedron;
            i++;
        }
        TNL_ASSERT_TRUE(i == 7, "incorrect number of elements in test with corners");
    };
    Algorithms::ParallelFor<DeviceType>::exec(0, 1, check_back_corner_ray_kernel);
}