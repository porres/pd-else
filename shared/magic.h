#define MAGIC_NAN 0x7FFFFFFFul

// returns float fields. Arguments: object/inlet number (0 indexed)
EXTERN t_float *obj_findsignalscalar(t_object *x, int m);

union magic_ui32_fl{
	uint32_t uif_uint32;
	t_float uif_float;
};

int magic_inlet_connection(t_object *x, t_glist *glist, int inno, t_symbol *outsym);

void magic_setnan (t_float *in);
int magic_isnan (t_float in);

