
typedef enum
{
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

typedef struct particle_s
{
	vec3_t		org;
	float		color;
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;

#ifdef GLQUAKE
void GL_DrawParticleInit(void);
void GL_DrawParticleBegin(void);
void GL_DrawParticleEnd(void);
void GL_DrawParticle(particle_t *p);
#endif

