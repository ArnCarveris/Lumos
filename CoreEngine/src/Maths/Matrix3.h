#pragma once

#include "JM.h"
#include "Vector3.h"
#include "MathsCommon.h"

namespace jm
{
	namespace maths {

		class Matrix4;

#ifdef JM_SSEMAT3

		class JM_EXPORT MEM_ALIGN Matrix3 {
		public:
			inline Matrix3() { ToIdentity(); }

			inline Matrix3(const float (&elements)[9]) { memcpy(values, elements, 9 * sizeof(float)); }

			Matrix3(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3);

			Matrix3(
					float a1, float a2, float a3,
					float b1, float b2, float b3,
					float c1, float c2, float c3);

			Matrix3(const Matrix4 &m4);

			union 
			{
				float values[9];
				struct 
				{
					float _11, _12, _13;
					float _21, _22, _23;
					float _31, _32, _33;
				};
			} MEM_ALIGN;

			//Set all matrix values to zero
			inline Matrix3 &ToZero() {
				memset(values, 0, 9 * sizeof(float));
				return *this;
			}

			inline Matrix3 &ToIdentity() {
				memcpy(values, Matrix3::IDENTITY_DATA, 9 * sizeof(float));
				return *this;
			}


			inline float operator[](int index) const {
				return values[index];
			}

			inline float &operator[](int index) {
				return values[index];
			}

			inline float operator()(int row, int col) const {
				return values[row + col * 3];
			}

			inline float &operator()(int row, int col) {
				return values[row + col * 3];
			}

			inline Vector3 GetRow(unsigned int row) const {
				return Vector3(values[row], values[row + 3], values[row + 6]);
			}

			inline Vector3 GetCol(unsigned int column) const {
				Vector3 out;
				memcpy(&out, &values[3 * column], sizeof(Vector3));
				return out;
			}

			inline void SetRow(unsigned int row, const Vector3 &val) {
				values[row] = val.GetX();
				values[row + 3] = val.GetY();
				values[row + 6] = val.GetZ();
			}

			inline void SetCol(unsigned int column, const Vector3 &val) {
				memcpy(&values[column * 3], &val, sizeof(Vector3));
			}

			inline Vector3 GetDiagonal() const { return Vector3(values[0], values[4], values[8]); }

			inline void SetDiagonal(const Vector3 &v) {
				values[0] = v.GetX();
				values[4] = v.GetY();
				values[8] = v.GetZ();
			}


			inline Vector3 operator*(const Vector3 &v) const {
				return Vector3(
						v.GetX() * values[0] + v.GetY() * values[3] + v.GetZ() * values[6],
						v.GetX() * values[1] + v.GetY() * values[4] + v.GetZ() * values[7],
						v.GetX() * values[2] + v.GetY() * values[5] + v.GetZ() * values[8]
				);
			};

			inline Matrix3 operator*(const Matrix3 &m) const {
				Matrix3 result;
				for (unsigned i = 0; i < 9; i += 3)
					for (unsigned j = 0; j < 3; ++j)
						result.values[i + j] = m.values[i] * values[j] + m.values[i + 1] * values[j + 3] +
											   m.values[i + 2] * values[j + 6];

				return result;
			};

			inline void Transpose() {
				__m128 empty = _mm_setzero_ps();
				__m128 column1 = _mm_loadu_ps(&values[0]);
				__m128 column2 = _mm_loadu_ps(&values[3]);
				__m128 column3 = _mm_loadu_ps(&values[6]);

				_MM_TRANSPOSE4_PS(column1, column2, column3, empty);

				_mm_storeu_ps(&values[0], column1);
				_mm_storeu_ps(&values[3], column2);
				values[6] = GetValue(column3, 0);
				values[7] = GetValue(column3, 1);
				values[8] = GetValue(column3, 2);
			}

			// Standard Matrix Functionality
			static Matrix3 Inverse(const Matrix3 &rhs);

			static Matrix3 Transpose(const Matrix3 &rhs);

			static Matrix3 Adjugate(const Matrix3 &m);

			float Determinant() const;

			Matrix3 Inverse() const;

			bool TryInvert();

			bool TryTransposedInvert();

			static const float EMPTY_DATA[9];
			static const float IDENTITY_DATA[9];

			static const Matrix3 EMPTY;
			static const Matrix3 IDENTITY;
			static const Matrix3 ZeroMatrix;

			static Matrix3 RotationX(float degrees);

			static Matrix3 RotationY(float degrees);

			static Matrix3 RotationZ(float degrees);

			//Creates a rotation matrix that rotates by 'degrees' around the 'axis'
			static Matrix3 Rotation(float degrees, const Vector3 &axis);

			static Matrix3 Rotation(float degreesX, float degreesY, float degreesZ);

			//Creates a scaling matrix (puts the 'scale' vector down the diagonal)
			static Matrix3 Scale(const Vector3 &scale);

			friend std::ostream &operator<<(std::ostream &o, const Matrix3 &m);

#ifdef JM_SSEMAT3
			MEM_ALIGN_NEW_DELETE;
#endif
		};

#else

        class JM_EXPORT Matrix3
        {
        public:

            static const Matrix3 Identity;
            static const Matrix3 ZeroMatrix;

            //ctor
            Matrix3();

            explicit Matrix3(const float elements[9]);

            Matrix3(const Vector3& c1, const Vector3& c2, const Vector3& c3);

            Matrix3(float a1, float a2, float a3,
                float b1, float b2, float b3,
                float c1, float c2, float c3);

            Matrix3(const Matrix4& mat44);

            ~Matrix3()
            {

            }

            //Default States
            void	ToZero();
            void	ToIdentity();

            //Default Accessors
            inline float   operator[](int index) const { return values[index]; }
            inline float&  operator[](int index) { return values[index]; }
            inline float   operator()(int row, int col) const { return values[row + col * 3]; }
            inline float&  operator()(int row, int col) { return values[row + col * 3]; }

            inline const Vector3&	GetCol(int idx) const { return *((Vector3*)&values[idx * 3]); }
            inline void				SetCol(int idx, const Vector3& row) { values[idx * 3] = row.GetX(); values[idx * 3 + 1] = row.GetY(); values[idx * 3 + 2] = row.GetZ(); }

            inline Vector3			GetRow(int idx)	const { return Vector3(values[idx], values[3 + idx], values[6 + idx]); }
            inline void				SetRow(int idx, const Vector3& col) { values[idx] = col.GetX(); values[3 + idx] = col.GetY(); values[6 + idx] = col.GetZ(); }

            //Common Matrix Properties
            inline Vector3			GetScalingVector() const { return Vector3(_11, _22, _33); }
            inline void				SetScalingVector(const Vector3& in) { _11 = in.GetX(), _22 = in.GetY(), _33 = in.GetZ(); }

            //Transformation Matrix
            static Matrix3 Rotation(float degrees, const Vector3 &axis);
            static Matrix3 Scale(const Vector3 &scale);

            // Standard Matrix Functionality
            static Matrix3 Inverse(const Matrix3& rhs);
            static Matrix3 Transpose(const Matrix3& rhs);
            static Matrix3 Adjugate(const Matrix3& m);

            static Matrix3 OuterProduct(const Vector3& a, const Vector3& b);

            // Additional Functionality
            float Trace() const;
            float Determinant() const;

            //Other representation of data.
            union
            {
                float	values[9];
                struct {
                    float _11, _12, _13;
                    float _21, _22, _23;
                    float _31, _32, _33;
                };
            };
        };

        Matrix3& operator+=(Matrix3& a, const Matrix3& rhs);
        Matrix3& operator-=(Matrix3& a, const Matrix3& rhs);

        Matrix3 operator+(const Matrix3& a, const Matrix3& rhs);
        Matrix3 operator-(const Matrix3& a, const Matrix3& rhs);
        Matrix3 operator*(const Matrix3& a, const Matrix3& rhs);

        Matrix3& operator+=(Matrix3& a, const float b);
        Matrix3& operator-=(Matrix3& a, const float b);
        Matrix3& operator*=(Matrix3& a, const float b);
        Matrix3& operator/=(Matrix3& a, const float b);

        Matrix3 operator+(const Matrix3& a, const float b);
        Matrix3 operator-(const Matrix3& a, const float b);
        Matrix3 operator*(const Matrix3& a, const float b);
        Matrix3 operator/(const Matrix3& a, const float b);

        Vector3 operator*(const Matrix3& a, const Vector3& b);

#endif
	}
}