/*
 * Compressive Sensing kernel for FPGA implementation
 */

#include <stdlib.h>
#include <math.h>

/*
 * ************************************************
 * Function: Tracer Forward 
 * Input:  source
 *         detector
 *         image
 * Output: Return Value
 * Return:  
 *************************************************
 */
DataType Ray_Tracer_Forward(Vector3D * source,
			    Vector3D * detector,
			    int N_x, int N_y, int N_z, Array3D * image)
{
	DataType sino = 0.0;
	DataType L;
	Vector3D Ray_Dir;
	Vector3D Len;

	DataType absvalue_x;
	DataType absvalue_y;
	DataType absvalue_z;
	int signx, signy, signz;
	DataType lambda_min;
	DataType lambda_max;

	Vector3DInt v, u;
	int index;
	DataType x_min;
	DataType y_min;
	DataType z_min;
	DataType lambda_0;
	Vector3D lambda;
	DataType Tx, Ty, Tz;
	DataType length;

	/* get direction */
	Vector3D_Sub(&Ray_Dir, detector, source);
	/* get ray length */
	L = Vector3D_Dot(&Ray_Dir, &Ray_Dir);
	L = SQRT(L);

	signx = (Ray_Dir.x > 0) ? 1 : -1;
	signy = (Ray_Dir.y > 0) ? 1 : -1;
	signz = (Ray_Dir.z > 0) ? 1 : -1;

	absvalue_x = ABS_VALUE(Ray_Dir.x);
	absvalue_y = ABS_VALUE(Ray_Dir.y);
	absvalue_z = ABS_VALUE(Ray_Dir.z);

	/* get x=1 Lx Ly Lz */
	Len.x = (DataType) ((absvalue_x > 1.e-4) ? (L / absvalue_x) : 1.e6);
	Len.y = (DataType) ((absvalue_y > 1.e-4) ? (L / absvalue_y) : 1.e6);
	Len.z = (DataType) ((absvalue_z > 1.e-4) ? (L / absvalue_z) : 1.e6);

	/* get the entry and exit point between Ray & Image
	 * distance between source and entry point */
	lambda_min = MAX(MAX(0.0,
			     MIN(LAMBDA_X(0, source->x, detector->x, L),
				 LAMBDA_X(N_x, source->x, detector->x, L))
			 ),
			 MAX(MIN
			     (LAMBDA_Y(0, source->y, detector->y, L),
			      LAMBDA_Y(N_y, source->y, detector->y, L)),
			     MIN(LAMBDA_Z(0, source->z, detector->z, L),
				 LAMBDA_Z(N_z, source->z, detector->z, L))
			 )
	    );

	/* distance between source and exit point */
	lambda_max = MIN(MIN(L,
			     MAX(LAMBDA_X(0, source->x, detector->x, L),
				 LAMBDA_X(N_x, source->x, detector->x, L))
			 ),
			 MIN(MAX
			     (LAMBDA_Y(0, source->y, detector->y, L),
			      LAMBDA_Y(N_y, source->y, detector->y, L)),
			     MAX(LAMBDA_Z(0, source->z, detector->z, L),
				 LAMBDA_Z(N_z, source->z, detector->z, L))
			 )
	    );

	/* no intersection between Ray & Image */
	if (lambda_min >= lambda_max)
		return sino;

	/* get the position of entry point */
	x_min = source->x + lambda_min * Ray_Dir.x / L;
	y_min = source->y + lambda_min * Ray_Dir.y / L;
	z_min = source->z + lambda_min * Ray_Dir.z / L;

	x_min = (x_min) > 0 ? x_min : 0;
	y_min = (y_min) > 0 ? y_min : 0;
	z_min = (z_min) > 0 ? z_min : 0;

	/* v current pixel, u next pixel */
	if (ABS_VALUE(z_min - (int)(z_min + 0.5)) < 1.e-4) {
		/* integer pixel */
		v.z = (int)(z_min + 0.5);
		u.z = v.z + signz;
		index = 3;
		if (Ray_Dir.z < 0.0)
			v.z -= 1;
	} else {
		/* frac pixel */
		v.z = (int)(z_min);
		if (Ray_Dir.z < 0.0)
			u.z = (int)(z_min);
		else
			u.z = (int)(z_min + 1);
	}

	if (ABS_VALUE(y_min - (int)(y_min + 0.5)) < 1.e-4) {
		v.y = (int)(y_min + 0.5);
		u.y = v.y + signy;
		index = 2;
		if (Ray_Dir.y < 0.0)
			v.y -= 1;
	} else {
		v.y = (int)(y_min);
		if (Ray_Dir.y < 0.0)
			u.y = (int)(y_min);
		else
			u.y = (int)(y_min + 1);
	}

	if (ABS_VALUE(x_min - (int)(x_min + 0.5)) < 1.e-4) {
		v.x = (int)(x_min + 0.5);
		u.x = v.x + signx;
		index = 1;
		if (Ray_Dir.x < 0.0)
			v.x -= 1;
	} else {
		v.x = (int)(x_min);
		if (Ray_Dir.x < 0.0)
			u.x = (int)(x_min);
		else
			u.x = (int)(x_min + 1);
	}

	/* set the beginning pixel
	 * lambda.x .y .z are the distance between source point and next ray point */
	lambda_0 = lambda_min;
	lambda.x =
	    (absvalue_x < 1e-4) ? 1e6 : LAMBDA_X(u.x, source->x, detector->x,
						 L);
	lambda.y =
	    (absvalue_y < 1e-4) ? 1e6 : LAMBDA_Y(u.y, source->y, detector->y,
						 L);
	lambda.z =
	    (absvalue_z < 1e-4) ? 1e6 : LAMBDA_Z(u.z, source->z, detector->z,
						 L);

	/* the main loop
	 * large distance on X axis */
	if (ABS_VALUE(Ray_Dir.x) > ABS_VALUE(Ray_Dir.y)) {
		/* intersection with x */
		if (index == 1) {
			/* align the x axis without intersection of y and z */
			if (MIN(lambda.z, lambda.y) > lambda_max) {
				while (1) {
					sino +=
					    ARRAY3DData(image, v.x, v.y, v.z);

					v.x += signx;

					if (v.x >= N_x || v.x < 0)
						return sino * Len.x;
				}
			}
			/* cross z firstly */
			if (lambda.z < lambda.y) {
				Ty = lambda.y - lambda.z;
				Tz = lambda.z - lambda_0;
				Tx = lambda.x - lambda_0;

				/* intersection Z firstly */
				if (Tz < Tx) {
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Tz;
					Tz = Len.z;

					/* update v.z */
					v.z += signz;

					if (v.z >= N_z || v.z < 0)
						return sino;
				} else {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tz -= Tx;
					Tx = Len.x;
					/* update v.x */
					v.x += signx;
					/* exit image */
					if (v.x >= N_x || v.x < 0)
						return sino;

					/* intersection with z */
					while (Tz >= Tx) {
						length = Tx;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tz -= Tx;
						v.x += signx;
						if (v.x >= N_x || v.x < 0)
							return sino;
					}

					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				}
			} else {
				Tz = lambda.z - lambda.y;
				Ty = lambda.y - lambda_0;
				Tx = lambda.x - lambda_0;

				if (Ty < Tx) {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
				} else {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;

					while (Ty >= Tx) {
						length = Tx;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Ty -= Tx;
						v.x += signx;
						if (v.x >= N_x || v.x < 0)
							return sino;
					}

					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
				}
			}
		} else if (index == 2) {
			/* intersection with y */
			Ty = Len.y;
			Tz = lambda.z - lambda_0;
			Tx = lambda.x - lambda_0;
		} else {
			/* intersection with z */
			Tz = Len.z;
			Ty = lambda.y - lambda_0;
			Tx = lambda.x - lambda_0;
		}

		/* intersection begin with Y, Z */
		while (1) {
			if (Tz < Ty) {
				/* intersection with Z firstly */
				Ty -= Tz;
				if (Tz < Tx) {
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				} else {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tz -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;

					while (Tz >= Tx) {
						length = Tx;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tz -= Tx;
						v.x += signx;
						if (v.x >= N_x || v.x < 0)
							return sino;
					}
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				}
			} else {
				/* intersection with Y firstly */
				Tz -= Ty;
				if (Ty < Tx) {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
				} else {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;

					while (Ty >= Tx) {
						length = Tx;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Ty -= Tx;
						v.x += signx;
						if (v.x >= N_x || v.x < 0)
							return sino;
					}

					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
				}
			}
		}
	} else {
		/* large distance on Y axis */
		/* intersection with y integer pixel */
		if (index == 2) {
			if (MIN(lambda.z, lambda.x) > lambda_max) {
				while (1) {
					sino +=
					    ARRAY3DData(image, v.x, v.y, v.z);

					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino * Len.y;
				}
			}
			if (lambda.z < lambda.x) {
				Tx = lambda.x - lambda.z;
				Tz = lambda.z - lambda_0;
				Ty = lambda.y - lambda_0;
				if (Tz < Ty) {
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				} else {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tz -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
					while (Tz >= Ty) {
						length = Ty;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tz -= Ty;
						v.y += signy;
						if (v.y >= N_y || v.y < 0)
							return sino;
					}
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				}
			} else {
				Tz = lambda.z - lambda.x;
				Tx = lambda.x - lambda_0;
				Ty = lambda.y - lambda_0;
				if (Tx < Ty) {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;
				} else {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
					while (Tx >= Ty) {
						length = Ty;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tx -= Ty;
						v.y += signy;
						if (v.y >= N_y || v.y < 0)
							return sino;
					}
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;
				}
			}
		} else if (index == 1) {
			Tx = Len.x;
			Tz = lambda.z - lambda_0;
			Ty = lambda.y - lambda_0;
		} else {
			Tz = Len.z;
			Tx = lambda.x - lambda_0;
			Ty = lambda.y - lambda_0;
		}

		while (1) {
			if (Tz < Tx) {
				Tx -= Tz;
				if (Tz < Ty) {
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				} else {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tz -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;
					while (Tz >= Ty) {
						length = Ty;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tz -= Ty;
						v.y += signy;
						if (v.y >= N_y || v.y < 0)
							return sino;
					}
					length = Tz;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tz;
					Tz = Len.z;
					v.z += signz;
					if (v.z >= N_z || v.z < 0)
						return sino;
				}
			} else {
				Tz -= Tx;
				if (Tx < Ty) {
					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;
				} else {
					length = Ty;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Tx -= Ty;
					Ty = Len.y;
					v.y += signy;
					if (v.y >= N_y || v.y < 0)
						return sino;

					while (Tx >= Ty) {
						length = Ty;
						sino +=
						    ARRAY3DData(image, v.x, v.y,
								v.z) * length;

						Tx -= Ty;
						v.y += signy;
						if (v.y >= N_y || v.y < 0)
							return sino;
					}

					length = Tx;
					sino +=
					    ARRAY3DData(image, v.x, v.y,
							v.z) * length;

					Ty -= Tx;
					Tx = Len.x;
					v.x += signx;
					if (v.x >= N_x || v.x < 0)
						return sino;
				}
			}
		}
	}
}

/*
 * ************************************************
 * Function: Forward Prejection 
 * Input:  iamge 
 *          
 * Output: sino  
 * Return:  
 *************************************************
 */
void Forward_Projection(Array3D * image, DataType D, int q, DataType ss, DataType d, DataType dtheta, DataType ssz, int qz, Array3D * sino)	//output 
{
	int N_x = image->index1Size;
	int N_y = image->index2Size;
	int N_z = image->index3Size;
	Vector3D source, detector;
	int NS = (int)(359.0 / dtheta);
	DataType thetaS = 0.;
	DataType dtheta2 = (DataType) (dtheta * PI / 180.);
	DataType ss2 = (DataType) (ss * PI / 180.);
	int i, j, k;

	for (i = 0; i <= NS; i++) {
		source.x = D * cos(thetaS) + d;
		source.y = D * sin(thetaS) + d;
		source.z = d;
		for (j = -q; j < q + 1; j++) {
			detector.x = D * cos(j * ss2 + PI + thetaS) + d;
			detector.y = D * sin(j * ss2 + PI + thetaS) + d;
			for (k = -qz; k < qz + 1; k++) {
				DataType aa;
				detector.z = k * ssz + d;
				aa = Ray_Tracer_Forward(&source, &detector, N_x,
							N_y, N_z, image);
				ARRAY3DData(sino, (j + q), (k + qz), i) = aa;
			}
		}
		thetaS += dtheta2;
	}
}
