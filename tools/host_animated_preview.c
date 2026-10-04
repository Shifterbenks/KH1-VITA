#include "kh1vita/kh1_mdls.h"
#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mdls_texture.h"
#include "kh1vita/kh1_mset.h"
#include "kh1vita/renderer.h"
#include "kh1vita/software_rasterizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MeshLayout {
    uint32_t vertex_count, triangle_count, vertex_base, triangle_base, texture_index;
} MeshLayout;

typedef struct Ctx {
    MeshLayout *layout;
    uint32_t mesh_count;
    Kh1MdlsMat4 *pose;
    uint32_t joint_count;
    Kh1VitaRenderVertex *vertices;
    Kh1VitaRenderTriangle *triangles;
} Ctx;

static int count_mesh(const Kh1MdlsMeshInfo *m, void *user) {
    Ctx *c = (Ctx *)user;
    if (!c || !m || m->mesh_index >= c->mesh_count) return 1;
    c->layout[m->mesh_index].vertex_count = m->vertex_count;
    c->layout[m->mesh_index].triangle_count = m->triangle_count;
    c->layout[m->mesh_index].texture_index = m->texture_index;
    return 0;
}
static int make_vertex(uint32_t mi, uint32_t vi, const Kh1MdlsVertex *v, void *user) {
    Ctx *c = (Ctx *)user; MeshLayout *m; Kh1VitaRenderVertex *d;
    if (!c || !v || mi >= c->mesh_count || v->joint_id >= c->joint_count) return 1;
    m = &c->layout[mi]; if (vi >= m->vertex_count) return 1;
    d = &c->vertices[m->vertex_base + vi];
    kh1_mdls_transform_point(&c->pose[v->joint_id], v->x, v->y, v->z, &d->x, &d->y, &d->z);
    d->u = v->u; d->v = v->v; return 0;
}
static int make_tri(uint32_t mi, uint32_t ti, const Kh1MdlsTriangle *t, void *user) {
    Ctx *c = (Ctx *)user; MeshLayout *m; Kh1VitaRenderTriangle *d;
    if (!c || !t || mi >= c->mesh_count) return 1;
    m = &c->layout[mi]; if (ti >= m->triangle_count) return 1;
    d = &c->triangles[m->triangle_base + ti];
    d->a=m->vertex_base+t->a; d->b=m->vertex_base+t->b; d->c=m->vertex_base+t->c;
    d->texture_index=m->texture_index; d->color=0xffffffffu; return 0;
}
static int save_ppm(const char *path, const uint32_t *fb, unsigned w, unsigned h) {
    FILE *f=fopen(path,"wb"); unsigned y,x; if(!f)return -1; fprintf(f,"P6\n%u %u\n255\n",w,h);
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            uint32_t pixel = fb[y * w + x];
            unsigned char rgb[3] = {
                (unsigned char)(pixel & 255u),
                (unsigned char)((pixel >> 8) & 255u),
                (unsigned char)((pixel >> 16) & 255u)
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return 0;
}
int main(int argc,char **argv){
    const unsigned W=480,H=272; Kh1MdlsInfo mdls; Kh1MdlsGeometryInfo geom; Kh1MdlsGeometryCallbacks cb={0};
    MeshLayout *layout=NULL; Kh1MdlsJointInfo *base=NULL,*animated=NULL; Kh1MdlsMat4 *bind=NULL,*pose=NULL;
    Kh1VitaRenderVertex *verts=NULL; Kh1VitaRenderTriangle *tris=NULL; Kh1VitaRenderTexture *tex=NULL; uint32_t *fb=NULL; float *z=NULL;
    uint32_t joints_n=0,i,vb=0,tb=0; Ctx c; Kh1SwView view; Kh1MsetMotionInfo motion; int rc=1; unsigned motion_index; float frame;
    if(argc!=6){fprintf(stderr,"usage: %s file.mdls file.mset motion frame out.ppm\n",argv[0]);return 2;}
    motion_index=(unsigned)strtoul(argv[3],NULL,0); frame=strtof(argv[4],NULL);
    if(kh1_mdls_scan(argv[1],&mdls,NULL,NULL)!=0||kh1_mdls_scan_geometry(argv[1],NULL,NULL,&geom)!=0)goto done;
    layout=calloc(geom.mesh_count,sizeof(*layout)); if(!layout)goto done; memset(&c,0,sizeof(c));c.layout=layout;c.mesh_count=geom.mesh_count;cb.mesh_done=count_mesh;
    if (kh1_mdls_scan_geometry(argv[1], &cb, &c, NULL) != 0) goto done;
    for (i = 0; i < geom.mesh_count; ++i) {
        layout[i].vertex_base = vb;
        layout[i].triangle_base = tb;
        vb += layout[i].vertex_count;
        tb += layout[i].triangle_count;
    }
    if(kh1_mdls_read_bind_pose(argv[1],NULL,NULL,0,&joints_n)!=0)goto done;
    base=calloc(joints_n,sizeof(*base));animated=calloc(joints_n,sizeof(*animated));bind=calloc(joints_n,sizeof(*bind));pose=calloc(joints_n,sizeof(*pose));
    verts=calloc(geom.vertex_count,sizeof(*verts));tris=calloc(geom.triangle_count,sizeof(*tris));tex=calloc(mdls.texture_count,sizeof(*tex));fb=calloc(W*H,sizeof(*fb));z=malloc(W*H*sizeof(*z));
    if(!base||!animated||!bind||!pose||!verts||!tris||!tex||!fb||!z)goto done;
    if(kh1_mdls_read_bind_pose(argv[1],base,bind,joints_n,&joints_n)!=0)goto done;
    if(kh1_mset_evaluate_motion(argv[2],motion_index,frame,base,joints_n,animated,&motion)!=0)goto done;
    if(kh1_mdls_build_pose(animated,joints_n,pose,joints_n)!=0)goto done;
    c.pose=pose;c.joint_count=joints_n;c.vertices=verts;c.triangles=tris;memset(&cb,0,sizeof(cb));cb.vertex=make_vertex;cb.triangle=make_tri;
    if(kh1_mdls_scan_geometry(argv[1],&cb,&c,NULL)!=0)goto done;
    for(i=0;i<mdls.texture_count;++i){Kh1MdlsDecodedTextureInfo inf;unsigned char *rgba;if(kh1_mdls_decode_texture_rgba(argv[1],i,NULL,0,&inf)!=0)goto done;rgba=malloc(inf.rgba_size);if(!rgba)goto done;if(kh1_mdls_decode_texture_rgba(argv[1],i,rgba,inf.rgba_size,&inf)!=0){free(rgba);goto done;}tex[i].width=inf.width;tex[i].height=inf.height;tex[i].pixels=(const uint32_t*)rgba;}
    view.center_y=76.29f;view.scale=1.65f;view.screen_cx=240.0f;view.screen_cy=136.0f;view.yaw_radians=0.0f;kh1_sw_clear(fb,W,H,W,z,0xff181818u);
    kh1_sw_draw_model(fb,W,H,W,z,verts,geom.vertex_count,tris,geom.triangle_count,tex,mdls.texture_count,&view);if(save_ppm(argv[5],fb,W,H)!=0)goto done;
    printf("ANIM motion=%u frame=%.2f/%u channels=%u static=%u keys=%u -> %s\n",motion_index,frame,(unsigned)motion.frame_count,(unsigned)motion.channel_count,(unsigned)motion.static_transform_count,(unsigned)motion.key_count,argv[5]);rc=0;
done:if(tex)for(i=0;i<mdls.texture_count;++i)free((void*)tex[i].pixels);free(layout);free(base);free(animated);free(bind);free(pose);free(verts);free(tris);free(tex);free(fb);free(z);return rc;
}
