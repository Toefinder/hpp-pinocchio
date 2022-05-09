///
/// Copyright (c) 2011 CNRS
/// Author: Florent Lamiraux
///
///

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

// This test
//   - builds a robot with various types of joints,
//   - randomly samples pairs of configurations,
//   - test consistency between hpp::pinocchio::difference and
//     hpp::pinocchio::integrate functions.

#define BOOST_TEST_MODULE tdevice

#include <Eigen/Geometry>
#include <boost/test/unit_test.hpp>
#include <hpp/pinocchio/configuration.hh>
#include <hpp/pinocchio/device.hh>
#include <hpp/pinocchio/humanoid-robot.hh>
#include <hpp/pinocchio/joint-collection.hh>
#include <hpp/pinocchio/joint.hh>
#include <hpp/pinocchio/liegroup-element.hh>
#include <hpp/pinocchio/liegroup.hh>
#include <hpp/pinocchio/simple-device.hh>
#include <pinocchio/algorithm/joint-configuration.hpp>

using namespace hpp::pinocchio;
typedef Eigen::AngleAxis<value_type> AngleAxis_t;
typedef Eigen::Quaternion<value_type> Quaternion_t;

vector_t slerp(vectorIn_t v0, vectorIn_t v1, const value_type t) {
  Quaternion_t q0(v0[3], v0[0], v0[1], v0[2]);
  Quaternion_t q1(v1[3], v1[0], v1[1], v1[2]);
  Quaternion_t q = q0.slerp(t, q1);
  vector_t res(4);
  res[0] = q.x();
  res[1] = q.y();
  res[2] = q.z();
  res[3] = q.w();
  return res;
}

AngleAxis_t aa(vectorIn_t v0, vectorIn_t v1) {
  Quaternion_t q0(v0[3], v0[0], v0[1], v0[2]);
  Quaternion_t q1(v1[3], v1[0], v1[1], v1[2]);
  return AngleAxis_t(q0 * q1.conjugate());
}

vector_t interpolation(hpp::pinocchio::DevicePtr_t robot, vectorIn_t q0,
                       vectorIn_t q1, int n) {
  const size_type rso3 = 3;
  bool print = false;

  vector_t q2(q0);
  vector_t v(robot->numberDof());
  hpp::pinocchio::difference(robot, q1, q0, v);

  /// Check that q1 - q0 is the same as (-q1) - q0
  vector_t mq1(q1);
  mq1.segment<4>(rso3) *= -1;
  vector_t mv(robot->numberDof());
  hpp::pinocchio::difference(robot, mq1, q0, mv);
  BOOST_CHECK(mv.isApprox(v));

  value_type u = 0;
  value_type step = (value_type)1 / (value_type)(n + 2);
  vector_t angle1(n + 2), angle2(n + 2);

  if (print) std::cout << "HPP  : ";
  for (int i = 0; i < n + 2; ++i) {
    u = 0 + i * step;
    interpolate(robot, q0, q1, u, q2);
    AngleAxis_t aa1(aa(q0.segment<4>(rso3), q2.segment<4>(rso3)));
    angle1[i] = aa1.angle();
    if (print) std::cout << aa1.angle() << ", ";
    if (print) std::cout << q2.segment<4>(rso3).transpose() << "\n";
  }
  if (print) std::cout << "\nEigen: ";
  for (int i = 0; i < n + 2; ++i) {
    u = 0 + i * step;
    vector_t eigen_slerp = slerp(q0.segment<4>(rso3), q1.segment<4>(rso3), u);
    AngleAxis_t aa2(aa(q0.segment<4>(rso3), eigen_slerp));
    angle2[i] = aa2.angle();
    if (print) std::cout << aa2.angle() << ", ";
    if (print) std::cout << eigen_slerp.transpose() << "\n";
  }
  if (print) std::cout << "\n";
  return angle1 - angle2;
}

typedef std::vector<DevicePtr_t> Robots_t;
Robots_t createRobots() {
  Robots_t r;
  r.push_back(unittest::makeDevice(unittest::HumanoidSimple));
  r.push_back(unittest::makeDevice(unittest::CarLike));
  r.push_back(unittest::makeDevice(unittest::ManipulatorArm2));
  return r;
}

const size_type NB_CONF = 4;
const size_type NB_SUCCESSIVE_INTERPOLATION = 1000;
const value_type eps = sqrt(Eigen::NumTraits<value_type>::dummy_precision());

BOOST_AUTO_TEST_CASE(is_valid_configuration) {
  DevicePtr_t robot;
  robot = unittest::makeDevice(unittest::HumanoidSimple);

  Configuration_t q = robot->neutralConfiguration();
  BOOST_CHECK(isNormalized(robot, q, eps));
  LiegroupElementConstRef P1(q, robot->configSpace());
  BOOST_CHECK(checkNormalized(P1, eps));

  /// Set a quaternion of norm != 1
  q[3] = 1;
  q[4] = 1;
  BOOST_CHECK(!isNormalized(robot, q, eps));
  LiegroupElementConstRef P2(q, robot->configSpace());
  BOOST_CHECK(!checkNormalized(P2, eps));

  robot = unittest::makeDevice(unittest::CarLike);

  q = robot->neutralConfiguration();
  BOOST_CHECK(isNormalized(robot, q, eps));
  LiegroupElementConstRef P3(q, robot->configSpace());
  BOOST_CHECK(checkNormalized(P3, eps));

  /// Set a complex of norm != 1
  q.segment<2>(2) << 1, 1;
  BOOST_CHECK(!isNormalized(robot, q, eps));
  LiegroupElementConstRef P4(q, robot->configSpace());
  BOOST_CHECK(!checkNormalized(P4, eps));

  robot = unittest::makeDevice(unittest::CarLike);

  q = robot->neutralConfiguration();
  BOOST_CHECK(isNormalized(robot, q, eps));
  LiegroupElementConstRef P5(q, robot->configSpace());
  BOOST_CHECK(checkNormalized(P5, eps));
}

void test_difference_and_distance(DevicePtr_t robot) {
  Configuration_t q0;
  q0.resize(robot->configSize());
  Configuration_t q1;
  q1.resize(robot->configSize());
  Configuration_t q2;
  q2.resize(robot->configSize());
  vector_t q1_minus_q0;
  q1_minus_q0.resize(robot->numberDof());
  for (size_type i = 0; i < NB_CONF; ++i) {
    q0 = ::pinocchio::randomConfiguration(robot->model());
    q1 = ::pinocchio::randomConfiguration(robot->model());

    BOOST_CHECK(isNormalized(robot, q0, eps));
    BOOST_CHECK(isNormalized(robot, q1, eps));

    difference<DefaultLieGroupMap>(robot, q1, q0, q1_minus_q0);

    // Check that the distance is the norm of the difference
    value_type d = distance(robot, q0, q1);

    BOOST_CHECK_MESSAGE(
        std::abs(d - q1_minus_q0.norm()) <
            Eigen::NumTraits<value_type>::dummy_precision(),
        "\n" << robot->name()
             << ": The distance is not the norm of the difference\n"
             << "q0=" << q0.transpose() << "\n"
             << "q1=" << q1.transpose() << "\n"
             << "(q1 - q0) = " << q1_minus_q0.transpose() << '\n'
             << "distance=" << d << "\n"
             << "(q1 - q0).norm () = " << q1_minus_q0.norm());
  }
}

BOOST_AUTO_TEST_CASE(difference_and_distance) {
  Robots_t robots = createRobots();
  for (std::size_t i = 0; i < robots.size(); ++i) {
    test_difference_and_distance(robots[i]);
  }
}

template <typename LieGroup>
void test_difference_and_integrate(DevicePtr_t robot) {
  Configuration_t q0;
  q0.resize(robot->configSize());
  Configuration_t q1;
  q1.resize(robot->configSize());
  Configuration_t q2;
  q2.resize(robot->configSize());
  vector_t q1_minus_q0;
  q1_minus_q0.resize(robot->numberDof());
  for (size_type i = 0; i < NB_CONF; ++i) {
    q0 = ::pinocchio::randomConfiguration(robot->model());
    q1 = ::pinocchio::randomConfiguration(robot->model());

    BOOST_CHECK(isNormalized(robot, q0, eps));
    BOOST_CHECK(isNormalized(robot, q1, eps));

    difference<LieGroup>(robot, q1, q0, q1_minus_q0);
    integrate<true, LieGroup>(robot, q0, q1_minus_q0, q2);

    BOOST_CHECK(isNormalized(robot, q2, eps));

    // Check that distance (q0 + (q1 - q0), q1) is zero
    BOOST_CHECK_MESSAGE(isApprox(robot, q1, q2, eps),
                        "\n(q0 + (q1 - q0)) is not equivalent to q1\n"
                            << "q1=" << q1.transpose() << "\n"
                            << "q2=" << q2.transpose() << "\n");
  }
}

BOOST_AUTO_TEST_CASE(difference_and_integrate) {
  Robots_t robots = createRobots();
  for (std::size_t i = 0; i < robots.size(); ++i) {
    test_difference_and_integrate<DefaultLieGroupMap>(robots[i]);
    test_difference_and_integrate<RnxSOnLieGroupMap>(robots[i]);
  }
}

template <typename LieGroup>
void test_interpolate_and_integrate(DevicePtr_t robot) {
  Configuration_t q0;
  q0.resize(robot->configSize());
  Configuration_t q1;
  q1.resize(robot->configSize());
  Configuration_t q2;
  q2.resize(robot->configSize());
  Configuration_t q3;
  q3.resize(robot->configSize());
  vector_t q1_minus_q0;
  q1_minus_q0.resize(robot->numberDof());
  const value_type eps_dist = value_type(robot->numberDof()) *
                              sqrt(Eigen::NumTraits<value_type>::epsilon());
  value_type d0, d1;
  for (size_type i = 0; i < NB_CONF; ++i) {
    q0 = ::pinocchio::randomConfiguration<LieGroup>(robot->model());
    q1 = ::pinocchio::randomConfiguration<LieGroup>(robot->model());

    BOOST_CHECK(isNormalized(robot, q0, eps));
    BOOST_CHECK(isNormalized(robot, q1, eps));

    difference<LieGroup>(robot, q1, q0, q1_minus_q0);

    interpolate<LieGroup>(robot, q0, q1, 0, q2);
    d0 = distance(robot, q0, q2);

    BOOST_CHECK_MESSAGE(d0 < eps_dist,
                        "\n(q0 + 0 * (q1 - q0)) is not equivalent to q0\n"
                            << "q0=" << q0.transpose() << "\n"
                            << "q1=" << q1.transpose() << "\n"
                            << "q2=" << q2.transpose() << "\n"
                            << "distance=" << d0);

    // vector_t errors = interpolation (robot, q0, q1, 4);
    // BOOST_CHECK_MESSAGE (errors.isZero (),
    // "The interpolation computed by HPP does not match the Eigen SLERP"
    // );

    interpolate<LieGroup>(robot, q0, q1, 1, q2);
    d1 = distance(robot, q1, q2);
    BOOST_CHECK_MESSAGE(d1 < eps_dist,
                        "\n(q0 + 1 * (q1 - q0)) is not equivalent to q1\n"
                            << "q0=" << q0.transpose() << "\n"
                            << "q1=" << q1.transpose() << "\n"
                            << "q2=" << q2.transpose() << "\n"
                            << "distance=" << d1);

    interpolate<LieGroup>(robot, q0, q1, 0.5, q2);
    // FIXME For SE(3) and SE(2) joints, with DefaultLieGroupMap
    // there is no guaranty that the interpolation
    // between two configuration within bounds will be within bounds.
    // So we should not saturate the configuration.
    integrate<false, LieGroup>(robot, q0, 0.5 * q1_minus_q0, q3);

    BOOST_CHECK(isNormalized(robot, q2, eps));
    BOOST_CHECK(isNormalized(robot, q3, eps));

    BOOST_CHECK_MESSAGE(isApprox(robot, q2, q3, eps),
                        "computation of q0 + 0.5 * (q1 - q0) does not yield "
                        "the same result with interpolate: "
                            << displayConfig(q2)
                            << " as with integrate: " << displayConfig(q3));

    d0 = distance(robot, q0, q2);
    d1 = distance(robot, q1, q2);
    BOOST_CHECK_MESSAGE(
        std::abs(d0 - d1) < eps_dist,
        "\nq2 = (q0 + 0.5 * (q1 - q0)) is not equivalent to q1\n"
            << "q0=" << q0.transpose() << "\n"
            << "q1=" << q1.transpose() << "\n"
            << "q2=" << q2.transpose() << "\n"
            << "distance=" << distance);
  }
}

BOOST_AUTO_TEST_CASE(interpolate_and_integrate) {
  Robots_t robots = createRobots();
  for (std::size_t i = 0; i < robots.size(); ++i) {
    test_interpolate_and_integrate<DefaultLieGroupMap>(robots[i]);
    test_interpolate_and_integrate<RnxSOnLieGroupMap>(robots[i]);
  }
}

void test_saturation(DevicePtr_t robot) {
  Configuration_t q0;
  q0.resize(robot->configSize());
  Configuration_t q1;
  q1.resize(robot->configSize());
  Configuration_t q2;
  q2.resize(robot->configSize());
  vector_t v;
  v.resize(robot->numberDof());
  v.setOnes();
  ArrayXb saturation(robot->numberDof());
  ArrayXb expected_sat(robot->numberDof());

  q0 = robot->model().upperPositionLimit;
  normalize(robot, q0);

  q1 = robot->model().lowerPositionLimit;
  normalize(robot, q1);

  BOOST_CHECK(isNormalized(robot, q0, eps));
  BOOST_CHECK(isNormalized(robot, q1, eps));

  integrate<false, DefaultLieGroupMap>(robot, q0, v, q2);
  saturate(robot, q2);
  BOOST_CHECK((q2.array() <= robot->model().upperPositionLimit.array()).all());
  BOOST_CHECK((q2.array() >= robot->model().lowerPositionLimit.array()).all());

  integrate<false, DefaultLieGroupMap>(robot, q1, -v, q2);
  saturate(robot, q2);
  BOOST_CHECK((q2.array() <= robot->model().upperPositionLimit.array()).all());
  BOOST_CHECK((q2.array() >= robot->model().lowerPositionLimit.array()).all());

  integrate<false, DefaultLieGroupMap>(robot, q0, v, q2);
  expected_sat = (q2.array() > robot->model().upperPositionLimit.array()) ||
                 (q2.array() < robot->model().lowerPositionLimit.array());
  BOOST_CHECK(saturate(robot, q2, saturation));
  BOOST_CHECK((q2.array() <= robot->model().upperPositionLimit.array()).all());
  BOOST_CHECK((q2.array() >= robot->model().lowerPositionLimit.array()).all());
  if (robot->numberDof() == robot->configSize()) {
    BOOST_CHECK_EQUAL(saturation.matrix(), expected_sat.matrix());
  } else {
    size_type iq = (robot->rootJoint()->configSize() == 4 ? 2 : 3);
    size_type sq = robot->rootJoint()->configSize();
    size_type sv = robot->rootJoint()->numberDof();

    BOOST_CHECK_EQUAL(saturation.head(iq).matrix(),
                      expected_sat.head(iq).matrix());
    BOOST_CHECK_EQUAL(saturation.tail(robot->numberDof() - sv).matrix(),
                      expected_sat.tail(robot->configSize() - sq).matrix());
  }

  integrate<false, DefaultLieGroupMap>(robot, q1, -v, q2);
  expected_sat = (q2.array() > robot->model().upperPositionLimit.array()) ||
                 (q2.array() < robot->model().lowerPositionLimit.array());
  BOOST_CHECK(saturate(robot, q2, saturation));
  BOOST_CHECK((q2.array() <= robot->model().upperPositionLimit.array()).all());
  BOOST_CHECK((q2.array() >= robot->model().lowerPositionLimit.array()).all());
  if (robot->numberDof() == robot->configSize()) {
    BOOST_CHECK_EQUAL(saturation.matrix(), expected_sat.matrix());
  } else {
    size_type iq = (robot->rootJoint()->configSize() == 4 ? 2 : 3);
    size_type sq = robot->rootJoint()->configSize();
    size_type sv = robot->rootJoint()->numberDof();

    BOOST_CHECK_EQUAL(saturation.head(iq).matrix(),
                      expected_sat.head(iq).matrix());
    BOOST_CHECK_EQUAL(saturation.tail(robot->numberDof() - sv).matrix(),
                      expected_sat.tail(robot->configSize() - sq).matrix());
  }
}

BOOST_AUTO_TEST_CASE(saturation) {
  Robots_t robots = createRobots();
  for (std::size_t i = 0; i < robots.size(); ++i) {
    test_saturation(robots[i]);
  }
}

template <typename LieGroup>
int test_successive_interpolation(DevicePtr_t robot) {
  int count = 0;
  Configuration_t q0(robot->configSize());
  Configuration_t q1(robot->configSize());

  vector_t q1_minus_q0(robot->numberDof());

  for (size_type i = 0; i < NB_CONF; ++i) {
    q0 = ::pinocchio::randomConfiguration(robot->model());

    for (size_type j = 0; j < NB_SUCCESSIVE_INTERPOLATION; ++j) {
      q1 = ::pinocchio::randomConfiguration(robot->model());
      // BOOST_CHECK(isNormalized(robot, q1, eps));
      if (!isNormalized(robot, q1, eps)) ++count;
      difference<LieGroup>(robot, q1, q0, q1_minus_q0);

      integrate<true, LieGroup>(robot, q0, 0.5 * q1_minus_q0, q0);
      // BOOST_CHECK(isNormalized(robot, q0, eps));
      if (!isNormalized(robot, q0, eps)) ++count;
    }
  }
  return count;
}

BOOST_AUTO_TEST_CASE(successive_interpolation) {
  Robots_t robots = createRobots();
  std::vector<int> results(2 * robots.size());
#pragma omp parallel for
  for (std::size_t i = 0; i < robots.size(); ++i) {
    results[2 * i] =
        test_successive_interpolation<DefaultLieGroupMap>(robots[i]);
    results[2 * i + 1] =
        test_successive_interpolation<RnxSOnLieGroupMap>(robots[i]);
  }
  for (std::size_t i = 0; i < robots.size(); ++i) {
    BOOST_CHECK_MESSAGE(
        results[2 * i] == 0,
        "DefaultLieGroupMap failed on robot " << robots[i]->name() << ".");
    BOOST_CHECK_MESSAGE(
        results[2 * i + 1] == 0,
        " RnxSOnLieGroupMap failed on robot " << robots[i]->name() << ".");
  }
}
