#pragma once

#include <cmath>
#include <limits>

#include <Eigen/Dense>

#include "drake/common/drake_assert.h"
#include "drake/common/drake_copyable.h"
#include "drake/common/eigen_types.h"
#include "drake/common/never_destroyed.h"
#include "drake/common/number_traits.h"
#include "drake/common/symbolic.h"

namespace drake {
namespace multibody {

/// This class represents a 3x3 rotation matrix between two arbitrary frames
/// A and B and helps ensure users create valid rotation matrices.  This class
/// relates right-handed orthogonal unit vectors Ax, Ay, Az fixed in frame A
/// to right-handed orthogonal unit vectors Bx, By, Bz fixed in frame B.
/// The monogram notation for the rotation matrix relating A to B is `R_AB`.
/// See @ref multibody_quantities for monogram notation for dynamics.
/// See @ref orientation_discussion "a discussion on rotation matrices".
///
/// @note This class does not store the frames associated with a rotation matrix
/// nor does it enforce strict proper usage of this class with vectors.
///
/// @tparam T The underlying scalar type. Must be a valid Eigen scalar.
///
/// Instantiated templates for the following kinds of T's are provided:
/// - double
/// - AutoDiffXd
///
// TODO(Mitiguy) Ensure this class handles RotationMatrix<symbolic::Expression>.
template <typename T>
class RotationMatrix {
 public:
  DRAKE_DEFAULT_COPY_AND_MOVE_AND_ASSIGN(RotationMatrix)

  /// Constructs a 3x3 identity %RotationMatrix -- which corresponds to
  /// aligning two frames (so that unit vectors Ax = Bx, Ay = By, Az = Bz).
  RotationMatrix() {}

  /// Constructs a %RotationMatrix from a Matrix3.
  /// @param[in] R an allegedly valid rotation matrix.
  /// @throws exception std::logic_error in debug builds if R fails IsValid(R).
  explicit RotationMatrix(const Matrix3<T>& R) : R_AB_() {
#ifdef DRAKE_ASSERT_IS_ARMED
    SetOrThrowIfNotValid(R);
#else
    SetUnchecked(R);
#endif
  }

  /// Makes the %RotationMatrix `R_AB` associated with rotating a frame B
  /// relative to a frame A by an angle `theta` about unit vector `Ax = Bx`.
  /// @param[in] theta radian measure of rotation angle about Ax.
  /// @note `R_AB` relates two frames A and B having unit vectors Ax, Ay, Az and
  /// Bx, By, Bz.  Initially, `Bx = Ax`, `By = Ay`, `Bz = Az`, then B undergoes
  /// a right-handed rotation relative to A by an angle `theta` about `Ax = Bx`.
  /// ```
  ///        ⎡ 1       0                 0  ⎤
  /// R_AB = ⎢ 0   cos(theta)   -sin(theta) ⎥
  ///        ⎣ 0   sin(theta)    cos(theta) ⎦
  /// ```
  static RotationMatrix<T> MakeXRotation(const T& theta) {
      Matrix3<T> R;
      using std::sin;
      using std::cos;
      const T c = cos(theta), s = sin(theta);
      // clang-format off
      R << 1,  0,  0,
           0,  c, -s,
           0,  s,  c;
      // clang-format on
    return RotationMatrix(R);
  }

  /// Makes the %RotationMatrix `R_AB` associated with rotating a frame B
  /// relative to a frame A by an angle `theta` about unit vector `Ay = By`.
  /// @param[in] theta radian measure of rotation angle about Ay.
  /// @note `R_AB` relates two frames A and B having unit vectors Ax, Ay, Az and
  /// Bx, By, Bz.  Initially, `Bx = Ax`, `By = Ay`, `Bz = Az`, then B undergoes
  /// a right-handed rotation relative to A by an angle `theta` about `Ay = By`.
  /// ```
  ///        ⎡  cos(theta)   0   sin(theta) ⎤
  /// R_AB = ⎢          0    1           0  ⎥
  ///        ⎣ -sin(theta)   0   cos(theta) ⎦
  /// ```
  static RotationMatrix<T> MakeYRotation(const T& theta) {
    Matrix3<T> R;
    using std::sin;
    using std::cos;
    const T c = cos(theta), s = sin(theta);
    // clang-format off
    R <<  c,  0,  s,
          0,  1,  0,
         -s,  0,  c;
    // clang-format on
    return RotationMatrix(R);
  }

  /// Makes the %RotationMatrix `R_AB` associated with rotating a frame B
  /// relative to a frame A by an angle `theta` about unit vector `Az = Bz`.
  /// @param[in] theta radian measure of rotation angle about Az.
  /// @note `R_AB` relates two frames A and B having unit vectors Ax, Ay, Az and
  /// Bx, By, Bz.  Initially, `Bx = Ax`, `By = Ay`, `Bz = Az`, then B undergoes
  /// a right-handed rotation relative to A by an angle `theta` about `Az = Bz`.
  /// ```
  ///        ⎡ cos(theta)  -sin(theta)   0 ⎤
  /// R_AB = ⎢ sin(theta)   cos(theta)   0 ⎥
  ///        ⎣         0            0    1 ⎦
  /// ```
  static RotationMatrix<T> MakeZRotation(const T& theta) {
    Matrix3<T> R;
    using std::sin;
    using std::cos;
    const T c = cos(theta), s = sin(theta);
    // clang-format off
    R << c, -s,  0,
         s,  c,  0,
         0,  0,  1;
    // clang-format on
    return RotationMatrix(R);
  }

  /// Makes the %RotationMatrix for a Body-fixed (intrinsic) Z-Y-X rotation by
  /// "yaw-pitch-roll" angles `[y, p, r]`, which is equivalent to a Space-fixed
  /// (extrinsic) X-Y-Z rotation by "roll-pitch-yaw angles" `[r, p, y]`.
  /// @param[in] ypr radian measures of three angles [yaw, pitch, roll].
  /// @note Denoting yaw `y`, pitch `p`, roll `r`, this method returns a
  /// rotation matrix `R_AD` equal to the matrix multiplication shown below.
  /// ```
  ///        ⎡cos(y) -sin(y)  0⎤   ⎡ cos(p)  0  sin(p)⎤   ⎡1      0        0 ⎤
  /// R_AD = ⎢sin(y)  cos(y)  0⎥ * ⎢     0   1      0 ⎥ * ⎢0  cos(r)  -sin(r)⎥
  ///        ⎣    0       0   1⎦   ⎣-sin(p)  0  cos(p)⎦   ⎣0  sin(r)   cos(r)⎦
  ///      =       R_AB          *        R_BC          *        R_CD
  /// ```
  /// Note: In this discussion, A is the Space frame and D is the Body frame.
  /// One way to visualize this rotation sequence is by introducing intermediate
  /// frames B and C (useful constructs to understand this rotation sequence).
  /// Initially, the frames are aligned so `Di = Ci = Bi = Ai (i = x, y, z)`.
  /// Then D is subjected to successive right-handed rotations relative to A.
  /// @li 1st rotation R_AB: Frames B, C, D collectively (as if welded together)
  /// rotate relative to frame A by a yaw angle `y` about `Az = Bz`.
  /// @li 2nd rotation R_BC: Frames C, D collectively (as if welded together)
  /// rotate relative to frame B by a pitch angle `p` about `By = Cy`.
  /// @li 3rd rotation R_CD: %Frame D rotates relative to frame C by a roll
  /// angle `r` about `Cx = Dx`.
  /// TODO(@mitiguy) Add Sherm/Goldstein's way to visualize rotation sequences.
  static RotationMatrix<T> MakeBodyZYXRotation(const Vector3<T>& ypr) {
    const Vector3<T> roll_pitch_yaw(ypr(2), ypr(1), ypr(0));
    return RotationMatrix<T>::MakeSpaceXYZRotation(roll_pitch_yaw);
  }

  /// Makes the %RotationMatrix for a Space-fixed (extrinsic) X-Y-Z rotation by
  /// "roll-pitch-yaw" angles `[r, p, y]`, which is equivalent to a Body-fixed
  /// (intrinsic) Z-Y-X rotation by "yaw-pitch-roll" angles `[y, p, r]`.
  /// @param[in] rpy radian measures of three angles [roll, pitch, yaw].
  /// @note Denoting roll `r`, pitch `p`, yaw `y`, this method returns a
  /// rotation matrix `R_AD` equal to the matrix multiplication shown below.
  /// ```
  ///        ⎡cos(y) -sin(y)  0⎤   ⎡ cos(p)  0  sin(p)⎤   ⎡1      0        0 ⎤
  /// R_AD = ⎢sin(y)  cos(y)  0⎥ * ⎢     0   1      0 ⎥ * ⎢0  cos(r)  -sin(r)⎥
  ///        ⎣    0       0   1⎦   ⎣-sin(p)  0  cos(p)⎦   ⎣0  sin(r)   cos(r)⎦
  ///      =       R_AB          *        R_BC          *        R_CD
  /// ```
  /// Note: In this discussion, A is the Space frame and D is the Body frame.
  /// One way to visualize this rotation sequence is by introducing intermediate
  /// frames B and C (useful constructs to understand this rotation sequence).
  /// Initially, the frames are aligned so `Di = Ci = Bi = Ai (i = x, y, z)`.
  /// Then D is subjected to successive right-handed rotations relative to A.
  /// @li 1st rotation R_CD: %Frame D rotates relative to frames C, B, A by a
  /// roll angle `r` about `Dx = Cx`.  Note: D and C are no longer aligned.
  /// @li 2nd rotation R_BC: Frames D, C (collectively -- as if welded together)
  /// rotate relative to frame B, A by a pitch angle `p` about `Cy = By`.
  /// Note: C and B are no longer aligned.
  /// @li 3rd rotation R_AB: Frames D, C, B (collectively -- as if welded)
  /// rotate relative to frame A by a roll angle `y` about `Bz = Az`.
  /// Note: B and A are no longer aligned.
  /// TODO(@mitiguy) Add Sherm/Goldstein's way to visualize rotation sequences.
  static RotationMatrix<T> MakeSpaceXYZRotation(const Vector3<T>& rpy) {
    Matrix3<T> R;
    using std::sin;
    using std::cos;
    const T c0 = cos(rpy(0)), s0 = sin(rpy(0));
    const T c1 = cos(rpy(1)), s1 = sin(rpy(1));
    const T c2 = cos(rpy(2)), s2 = sin(rpy(2));
    // clang-format off
    R << c2 * c1,  c2 * s1 * s0 - s2 * c0,  c2 * s1 * c0 + s2 * s0,
         s2 * c1,  s2 * s1 * s0 + c2 * c0,  s2 * s1 * c0 - c2 * s0,
        -s1,            c1 * s0,                 c1 * c0;
    // clang-format on
    return RotationMatrix(R);
  }

  /// Creates a %RotationMatrix templatized on a scalar type U from a
  /// %RotationMatrix templatized on scalar type T.  For example,
  /// ```
  /// RotationMatrix<double> source = RotationMatrix<double>::Identity();
  /// RotationMatrix<AutoDiffXd> foo = source.cast<AutoDiffXd>();
  /// ```
  /// @tparam U Scalar type on which the returned %RotationMatrix is templated.
  /// @note `RotationMatrix<From>::cast<To>()` creates a new
  /// `RotationMatrix<To>` from a `RotationMatrix<From>` but only if
  /// type `To` is constructible from type `From`.
  /// This cast method works in accordance with Eigen's cast method for Eigen's
  /// %Matrix3 that underlies this %RotationMatrix.  For example, Eigen
  /// currently allows cast from type double to AutoDiffXd, but not vice-versa.
  template <typename U>
  RotationMatrix<U> cast() const {
    const Matrix3<U> m = R_AB_.template cast<U>();
    return RotationMatrix<U>(m, true);
  }

  /// Sets `this` %RotationMatrix from a Matrix3.
  /// @param[in] R an allegedly valid rotation matrix.
  /// @throws exception std::logic_error in debug builds if R fails IsValid(R).
  void SetOrThrowIfNotValid(const Matrix3<T>& R) {
    ThrowIfNotValid(R);
    SetUnchecked(R);
  }

  /// Returns the 3x3 identity %RotationMatrix.
  /// @returns the 3x3 identity %RotationMatrix.
  // @internal This method's name was chosen to mimic Eigen's Identity().
  static const RotationMatrix<T>& Identity() {
    static const never_destroyed<RotationMatrix<T>> kIdentity;
    return kIdentity.access();
  }

  /// Forms `R_BA = R_AB⁻¹`, the inverse (transpose) of this %RotationMatrix.
  /// @retval `R_BA = R_AB⁻¹`, the inverse (transpose) of this %RotationMatrix.
  /// @note For a valid rotation matrix `R_BA = R_AB⁻¹ = R_ABᵀ`.
  // @internal This method's name was chosen to mimic Eigen's inverse().
  RotationMatrix<T> inverse() const {
    return RotationMatrix<T>(R_AB_.transpose());
  }

  /// Returns the Matrix3 underlying a %RotationMatrix.
  /// @returns the Matrix3 underlying a %RotationMatrix.
  const Matrix3<T>& matrix() const { return R_AB_; }

  /// In-place multiply of `this` rotation matrix `R_AB` by `other` rotation
  /// matrix `R_BC`.  On return, `this` is set to equal `R_AB * R_BC`.
  /// @param[in] other %RotationMatrix that post-multiplies `this`.
  /// @returns `this` rotation matrix which has been multiplied by `other`.
  /// @note It is possible (albeit improbable) to create an invalid rotation
  /// matrix by accumulating round-off error with a large number of multiplies.
  RotationMatrix<T>& operator*=(const RotationMatrix<T>& other) {
    SetUnchecked(matrix() * other.matrix());
    return *this;
  }

  /// Calculates `this` rotation matrix `R_AB` multiplied by `other` rotation
  /// matrix `R_BC`, returning the composition `R_AB * R_BC`.
  /// @param[in] other %RotationMatrix that post-multiplies `this`.
  /// @returns rotation matrix that results from `this` multiplied by `other`.
  /// @note It is possible (albeit improbable) to create an invalid rotation
  /// matrix by accumulating round-off error with a large number of multiplies.
  RotationMatrix<T> operator*(const RotationMatrix<T>& other) const {
    return RotationMatrix<T>(matrix() * other.matrix(), true);
  }

  /// Calculates `this` rotation matrix R multiplied by an arbitrary Vector3.
  /// @param[in] v 3x1 vector that post-multiplies `this`.
  /// @returns 3x1 vector that results from `R * v`.
  Vector3<T> operator*(const Vector3<T>& v) const {
    return Vector3<T>(matrix() * v);
  }

  /// Returns how close the matrix R is to to being a 3x3 orthonormal matrix by
  /// computing `‖R ⋅ R⁻¹ - I‖∞` (i.e., the maximum absolute value of the
  /// difference between the elements of R ⋅ R⁻¹ and the 3x3 identity matrix).
  /// @param[in] R matrix being checked for orthonormality.
  /// @returns `‖R ⋅ R⁻¹ - I‖∞`
  static T GetMeasureOfOrthonormality(const Matrix3<T>& R) {
    const Matrix3<T> m = R * R.transpose();
    return GetMaximumAbsoluteDifference(m, Matrix3<T>::Identity());
  }

  /// Tests if the determinant of a generic Matrix3 is positive or negative.
  /// @param[in] R an allegedly valid rotation matrix.
  /// @returns `true` if the determinant of R is positive.
  /// @internal The determinant of a proper rotation matrix is +1, whereas for
  /// an improper rotation matrix it is -1.  To avoid testing near +1 (which
  /// can return a false negative if the matrix is slightly non-orthonormal),
  /// all that is needed is to ensure the matrix is reasonably proper is to
  /// ensure the determinant is positive.  If the determinant is 0, it means
  /// the basis vectors are not linearly independent (do not span 3D space).
  static bool IsDeterminantPositive(const Matrix3<T>& R) {
    return R.determinant() > 0;
  }

  /// Tests if a generic Matrix3 has orthonormal vectors to within the threshold
  /// specified by `tolerance`.
  /// @param[in] R an allegedly orthonormal rotation matrix.
  /// @param[in] tolerance maximum allowable absolute difference between R * R⁻¹
  /// and the identity matrix I, i.e., checks if `‖R ⋅ R⁻¹ - I‖∞ <= tolerance`.
  /// @returns `true` if R is an orthonormal matrix.
  static bool IsOrthonormal(const Matrix3<T>& R, double tolerance) {
    return GetMeasureOfOrthonormality(R) <= tolerance;
  }

  /// Tests if a generic Matrix3 seems to be a proper orthonormal rotation
  /// matrix to within the threshold specified by `tolerance`.
  /// @param[in] R an allegedly valid rotation matrix.
  /// @param[in] tolerance maximum allowable absolute difference of `R * R⁻¹`
  /// and the identity matrix I (i.e., checks if `‖R ⋅ R⁻¹ - I‖∞ <= tolerance`).
  /// @returns `true` if R is a valid rotation matrix.
  static bool IsValid(const Matrix3<T>& R, double tolerance) {
    return IsOrthonormal(R, tolerance) && IsDeterminantPositive(R);
  }

  /// Tests if a generic Matrix3 is a proper orthonormal rotation matrix to
  /// within the threshold of get_internal_tolerance_for_orthonormality().
  /// @param[in] R an allegedly valid rotation matrix.
  /// @returns `true` if R is a valid rotation matrix.
  static bool IsValid(const Matrix3<T>& R) {
    return IsValid(R, get_internal_tolerance_for_orthonormality());
  }

  /// Tests if `this` rotation matrix R is a proper orthonormal rotation matrix
  /// to within the threshold of get_internal_tolerance_for_orthonormality().
  /// @returns `true` if `this` is a valid rotation matrix.
  bool IsValid() const { return IsValid(matrix()); }

  /// Returns `true` if `this` is exactly equal to the identity matrix.
  /// @returns `true` if `this` is exactly equal to the identity matrix.
  bool IsExactlyIdentity() const { return matrix() == Matrix3<T>::Identity(); }

  /// Returns true if `this` is within tolerance of the identity matrix.
  /// @returns `true` if `this` is equal to the identity matrix to within the
  /// threshold of get_internal_tolerance_for_orthonormality().
  bool IsIdentityToInternalTolerance() const {
    return IsNearlyEqualTo(matrix(), Matrix3<T>::Identity(),
                           get_internal_tolerance_for_orthonormality());
  }

  /// Compares each element of `this` to the corresponding element of `other`
  /// to check if they are the same to within a specified `tolerance`.
  /// @param[in] other %RotationMatrix to compare to `this`.
  /// @param[in] tolerance maximum allowable absolute difference between the
  /// matrix elements in `this` and `other`.
  /// @returns `true` if `‖this - other‖∞ <= tolerance`.
  bool IsNearlyEqualTo(const RotationMatrix<T>& other, double tolerance) const {
    return IsNearlyEqualTo(matrix(), other.matrix(), tolerance);
  }

  /// Compares each element of `this` to the corresponding element of `other`
  /// to check if they are exactly the same.
  /// @param[in] other %RotationMatrix to compare to `this`.
  /// @returns true if each element of `this` is exactly equal to the
  /// corresponding element in `other`.
  bool IsExactlyEqualTo(const RotationMatrix<T>& other) const {
    return matrix() == other.matrix();
  }

  /// Computes the infinity norm of `this` - `other` (i.e., the maximum absolute
  /// value of the difference between the elements of `this` and `other`).
  /// @param[in] other %RotationMatrix to subtract from `this`.
  /// @returns `‖`this` - `other`‖∞`
  T GetMaximumAbsoluteDifference(const RotationMatrix<T>& other) const {
    return GetMaximumAbsoluteDifference(matrix(), other.matrix());
  }

  /// Given an approximate rotation matrix M, finds the %RotationMatrix R
  /// closest to M.  Closeness is measured with a matrix-2 norm (or equivalently
  /// with a Frobenius norm).  Hence, this method creates a %RotationMatrix R
  /// from a 3x3 matrix M by minimizing `‖R - M‖₂` (the matrix-2 norm of (R-M))
  /// subject to `R * Rᵀ = I`, where I is the 3x3 identity matrix.  For this
  /// problem, closeness can also be measured by forming the orthonormal matrix
  /// R whose elements minimize the double-summation `∑ᵢ ∑ⱼ (R(i,j) - M(i,j))²`
  /// where `i = 1:3, j = 1:3`, subject to `R * Rᵀ = I`.  The square-root of
  /// this double-summation is called the Frobenius norm.
  /// @param[in] M a 3x3 matrix.
  /// @param[out] quality_factor.  The quality of M as a rotation matrix.
  /// `quality_factor` = 1 is perfect (M = R). `quality_factor` = 1.25 means
  /// that when M multiplies a unit vector (magnitude 1), a vector of magnitude
  /// as large as 1.25 may result.  `quality_factor` = 0.8 means that when M
  /// multiplies a unit vector, a vector of magnitude as small as 0.8 may
  /// result.  `quality_factor` = 0 means M is singular, so at least one of the
  /// bases related by matrix M does not span 3D space (when M multiples a unit
  /// vector, a vector of magnitude as small as 0 may result).
  /// @returns proper orthonormal matrix R that is closest to M.
  /// @throws exception std::logic_error if R fails IsValid(R).
  /// @note William Kahan (UC Berkeley) and Hongkai Dai (Toyota Research
  /// Institute) proved that for this problem, the same R that minimizes the
  /// Frobenius norm also minimizes the matrix-2 norm (a.k.a an induced-2 norm),
  /// which is defined [Dahleh, Section 4.2] as the column matrix u which
  /// maximizes `‖(R - M) u‖ / ‖u‖`, where `u ≠ 0`.  Since the matrix-2 norm of
  /// any matrix A is equal to the maximum singular value of A, minimizing the
  /// matrix-2 norm of (R - M) is equivalent to minimizing the maximum singular
  /// value of (R - M).
  ///
  /// - [Dahleh] "Lectures on Dynamic Systems and Controls: Electrical
  /// Engineering and Computer Science, Massachusetts Institute of Technology"
  /// https://ocw.mit.edu/courses/electrical-engineering-and-computer-science/6-241j-dynamic-systems-and-control-spring-2011/readings/MIT6_241JS11_chap04.pdf
  // @internal This function is not generated for symbolic Expression.
  // @internal This function's name is referenced in Doxygen documentation.
  template <typename S = T>
  static typename std::enable_if<is_numeric<S>::value, RotationMatrix<S>>::type
  ProjectToRotationMatrix(const Matrix3<S>& M, T* quality_factor = NULL) {
    const Matrix3<S> M_orthonormalized =
        ProjectMatrix3ToOrthonormalMatrix3(M, quality_factor);
    ThrowIfNotValid(M_orthonormalized);
    return RotationMatrix<S>(M_orthonormalized, true);
  }

  /// Returns an internal tolerance that checks rotation matrix orthonormality.
  /// @returns internal tolerance (small multiplier of double-precision epsilon)
  /// used to check whether or not a rotation matrix is orthonormal.
  /// @note The tolerance is chosen by developers to ensure a reasonably
  /// valid (orthonormal) rotation matrix.
  /// @note To orthonormalize a 3x3 matrix, use ProjectToRotationMatrix().
  static double get_internal_tolerance_for_orthonormality() {
    return kInternalToleranceForOrthonormality_;
  }

 private:
  // Make RotationMatrix<U> templatized on any typename U be a friend of a
  // %RotationMatrix templatized on any other typename T.
  // This is needed for the method RotationMatrix<T>::cast<U>() to be able to
  // use the necessary private constructor.
  template <typename U>
  friend class RotationMatrix;

  // Declares the allowable tolerance (small multiplier of double-precision
  // epsilon) used to check whether or not a rotation matrix is orthonormal.
  static constexpr double kInternalToleranceForOrthonormality_{
      128 * std::numeric_limits<double>::epsilon() };

  // Constructs a %RotationMatrix from a Matrix3.  No check is performed to test
  // whether or not the parameter R is a valid rotation matrix.
  // @param[in] R an allegedly valid rotation matrix.
  // @note The second parameter is just a dummy to distinguish this constructor
  // from one of the public constructors.
  RotationMatrix(const Matrix3<T>& R, bool) : R_AB_(R) {}

  // Sets `this` %RotationMatrix from a Matrix3.  No check is performed to
  // test whether or not the parameter R is a valid rotation matrix.
  // @param[in] R an allegedly valid rotation matrix.
  void SetUnchecked(const Matrix3<T>& R) { R_AB_ = R; }

  // Computes the infinity norm of R - `other` (i.e., the maximum absolute
  // value of the difference between the elements of R and `other`).
  // @param[in] R matrix from which `other` is subtracted.
  // @param[in] other matrix to subtract from R.
  // @returns `‖R - `other`‖∞`
  static T GetMaximumAbsoluteDifference(const Matrix3<T>& R,
                                        const Matrix3<T>& other) {
    const Matrix3<T> R_difference = R - other;
    return R_difference.template lpNorm<Eigen::Infinity>();
  }

  // Compares corresponding elements in two matrices to check if they are the
  // same to within a specified `tolerance`.
  // @param[in] R matrix to compare to `other`.
  // @param[in] other matrix to compare to R.
  // @param[in] tolerance maximum allowable absolute difference between the
  // matrix elements in R and `other`.
  // @returns `true` if `‖R - `other`‖∞ <= tolerance`.
  static bool IsNearlyEqualTo(const Matrix3<T>& R, const Matrix3<T>& other,
                              double tolerance) {
    const T R_max_difference = GetMaximumAbsoluteDifference(R, other);
    return R_max_difference <= tolerance;
  }

  // Throws an exception if R is not a valid %RotationMatrix.
  // @param[in] R an allegedly valid rotation matrix.
  static void ThrowIfNotValid(const Matrix3<T>& R) {
    if (!IsOrthonormal(R, get_internal_tolerance_for_orthonormality()))
      throw std::logic_error("Error: Rotation matrix is not orthonormal.");
    if (!IsDeterminantPositive(R))
      throw std::logic_error("Error: Rotation matrix determinant is negative. "
                             "It is possible a basis is left-handed");
  }

  // Given an approximate rotation matrix M, finds the orthonormal matrix R
  // closest to M.  Closeness is measured with a matrix-2 norm (or equivalently
  // with a Frobenius norm).  Hence, this method creates an orthonormal matrix R
  // from a 3x3 matrix M by minimizing `‖R - M‖₂` (the matrix-2 norm of (R-M))
  // subject to `R * Rᵀ = I`, where I is the 3x3 identity matrix.  For this
  // problem, closeness can also be measured by forming the orthonormal matrix R
  // whose elements minimize the double-summation `∑ᵢ ∑ⱼ (R(i,j) - M(i,j))²`
  // where `i = 1:3, j = 1:3`, subject to `R * Rᵀ = I`.  The square-root of
  // this double-summation is called the Frobenius norm.
  // @param[in] M a 3x3 matrix.
  // @param[out] quality_factor.  The quality of M as a rotation matrix.
  // `quality_factor` = 1 is perfect (M = R). `quality_factor` = 1.25 means
  // that when M multiplies a unit vector (magnitude 1), a vector of magnitude
  // as large as 1.25 may result.  `quality_factor` = -1 means M relates two
  // perfectly orthonormal bases, but one is right-handed whereas the other is
  // left-handed (M is a "reflection").  `quality_factor` = -0.8 means M
  // relates a right-handed basis to a left-handed basis and when M multiplies
  // a unit vector, a vector of magnitude as small as 0.8 may result.
  // `quality_factor` = 0 means M is singular, so at least one of the bases
  // related by matrix M does not span 3D space (when M multiples a unit vector,
  // a vector of magnitude as small as 0 may result).
  // @return orthonormal matrix R that is closest to M (note det(R) may be -1).
  // @note The SVD part of this algorithm can be derived as a modification of
  // section 3.2 in http://haralick.org/conferences/pose_estimation.pdf.
  // @note William Kahan (UC Berkeley) and Hongkai Dai (Toyota Research
  // Institute) proved that for this problem, the same R that minimizes the
  // Frobenius norm also minimizes the matrix-2 norm (a.k.a an induced-2 norm),
  // which is defined [Dahleh, Section 4.2] as the column matrix u which
  // maximizes `‖(R - M) u‖ / ‖u‖`, where `u ≠ 0`.  Since the matrix-2 norm of
  // any matrix A is equal to the maximum singular value of A, minimizing the
  // matrix-2 norm of (R - M) is equivalent to minimizing the maximum singular
  // value of (R - M).
  //
  // - [Dahleh] "Lectures on Dynamic Systems and Controls: Electrical
  // Engineering and Computer Science, Massachusetts Institute of Technology"
  // https://ocw.mit.edu/courses/electrical-engineering-and-computer-science/6-241j-dynamic-systems-and-control-spring-2011/readings/MIT6_241JS11_chap04.pdf
  template <typename Derived>
  static Matrix3<typename Derived::Scalar> ProjectMatrix3ToOrthonormalMatrix3(
      const Eigen::MatrixBase<Derived>& M, T* quality_factor) {
    DRAKE_DEMAND(M.rows() == 3 && M.cols() == 3);
    const auto svd = M.jacobiSvd(Eigen::ComputeFullU | Eigen::ComputeFullV);
    if (quality_factor != nullptr) {
      // Singular values are always non-negative and sorted in decreasing order.
      const auto singular_values = svd.singularValues();
      const T s_max = singular_values(0);  // maximum singular value.
      const T s_min = singular_values(2);  // minimum singular value.
      const T s_f = (s_max != 0.0 && s_min < 1.0/s_max) ? s_min : s_max;
      const T det = M.determinant();
      const double sign_det = (det > 0.0) ? 1 : ((det < 0.0) ? -1 : 0);
      *quality_factor = s_f * sign_det;
    }
    return svd.matrixU() * svd.matrixV().transpose();
  }

  // Stores the underlying rotation matrix relating two frames (e.g. A and B).
  // The default initialization is the identity matrix.
  Matrix3<T> R_AB_{Matrix3<T>::Identity()};
};

}  // namespace multibody
}  // namespace drake
