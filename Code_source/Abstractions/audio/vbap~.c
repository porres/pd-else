
#include "m_pd.h"
#include <stdlib.h>
#include <math.h>

#define   Nmax       16     // Max n speakers
#define   Ndefaut    4      // def n speakers
#define   Xdefaut    0
#define   Ydefaut    1
#define   Phidefaut  1.5708F
#define   Rdefaut    1      // def source distance
#define   Rdisq      0.5     // Rayon du disque central par défaut,
#define   T_COS      4096   // Taille du tableau cosinus (puissance de 2),
#define   MASQUE     4095   // Masque pour le modulo = T_COS - 1
#define   Pi         3.1415926535897932384F  //   = Pi
#define   I360       0.0027777777777777778F  //   = 1/360
#define   EPSILON    0.000001

static t_class *vbap_class;

typedef struct _vbap{
    t_object    x_obj;
    int	        x_cartesian;	// 1->cart 0->pol
    int         Nout;
    int         N;              // N speakers
	
    float 	    x, y;	        // cart coords
    float       phi, r;         // pol coords

    float       r_c;            // Radius from center
	
    float	    G[Nmax];	    // speaker gains
    float       teta[Nmax];     // Angles au centre des haut-parleurs,
    float       x_hp[Nmax];     // Abscisse de chaque haut-parleur,
    float       y_hp[Nmax];     // Ordonnée des haut-parleurs,
    float       dst_hp[Nmax];   // speakers distance

    float       Gstop[Nmax];    // Cible pour l'interpolation,

    float       G0[Nmax];       // gains des hp pour la position centrale,
    float       L[Nmax][2][2];  // Tableaux contenant toute les matrices
                                // inversées des configurations de 2 haut-parleurs,
    int         rev[Nmax];      // Tableau qui permet d'ordonner les hp
                                // par ordre croissant des angles,
    float      *cosin;          // Adresse des tableaux pour les cosinus,
}t_vbap;

float vbap_angle_wrap(float f){
    if(f >= 0 && f < 360 )
        return(f);
    while(f < 0)
        f += 360;
    while(f >= 360)
        f -= 360;
    return(f);
}

void vbap_set_matrix( t_vbap *x ){  // set speaker matrix
    float xhp1, yhp1, xhp2, yhp2;   // Coordonnées provisoires des hp,
    int   hp, i, j;                 // indices de boucles,
    int   itemp; float ftemp;       // entier et flottant temporaire,
    float g[Nmax];                  // gains de la position centrale,
    float Puis, iAmp;               // puissance et amplitude des gains,
    float pteta, pdist;             // angle et distance porovisoires,
    float g1, g2;                   // gains provisoires.
    float px[4] = {1, 0, -1, 0};    // positions des 4 points pour la position centrale
    float py[4] = {0, 1, 0, -1};
    for(i = 0; i < Nmax; i++){      // init tables
        x->rev[i] = i;
        g[i] = 0;
    }
    // Arrangement du tableau rev qui donne l'ordre croissant des angles des hp:
    // ordre croissant -> ordre de création.
    for(j = 0; j < x->N; j++){
        for(i = 0; i < x->N-1; i++){
            if(x->teta[x->rev[i]] > x->teta[x->rev[i+1]]){
                itemp = x->rev[i];
                x->rev[i] = x->rev[i+1];
                x->rev[i+1] = itemp;
            }
        }
    }
    // Calculate speaker distances
    for(hp = 0; hp < x->N; hp++)
        x->dst_hp[hp] = (float)sqrt(x->x_hp[hp]*x->x_hp[hp] + x->y_hp[hp]*x->y_hp[hp]);
    // Calcul des matrices inversŽes, dans l'ordre des téta croissants :
    for(hp = 0; hp < x->N; hp++){
        // Coodonnées des hp ramenés sur le cercle
        xhp1 = x->x_hp[x->rev[hp]] / x->dst_hp[x->rev[hp]];
        yhp1 = x->y_hp[x->rev[hp]] / x->dst_hp[x->rev[hp]];
        xhp2 = x->x_hp[x->rev[(hp+1)%x->N]] / x->dst_hp[x->rev[(hp+1)%x->N]];
        yhp2 = x->y_hp[x->rev[(hp+1)%x->N]] / x->dst_hp[x->rev[(hp+1)%x->N]];
        // Calcul du dénominateur
        ftemp = xhp1 * yhp2 - yhp1 * xhp2;
        // Si le dénominateur est nul, c'est que 2 hp consécutifs
        // sont alignés, c'est pas possible.
        if(fabs(ftemp) <= EPSILON){
            post("[vbap~]: 2 aligned speakers is not possible");
            ftemp = 0;
        }
        else
            ftemp = 1/ftemp;
        // Calcul de la matrice inverse associée aux 2 hp:
        x->L[hp][0][0] =  ftemp*yhp2;
        x->L[hp][0][1] = -ftemp*xhp2;
        x->L[hp][1][0] = -ftemp*yhp1;
        x->L[hp][1][1] =  ftemp*xhp1;
    }
    for(i = 0; i < 4; i++){ // get gains of central position
        pdist = (float)sqrt( px[i]*px[i] + py[i]*py[i]);
        pteta = (float)acos( px[i]/pdist );
        if(py[i] < 0)
            pteta = 2*Pi - pteta;
        // Recherche des hp encadrant le point:
        for(hp = 0; hp < x->N-1; hp++){
            if(pteta >= x->teta[x->rev[hp]] && pteta < x->teta[x->rev[(hp+1)%x->N]] )
                break;
        };
        // Calcul des gains:
        g1 = x->L[hp][0][0]*px[i] + x->L[hp][0][1]*py[i];
        g2 = x->L[hp][1][0]*px[i] + x->L[hp][1][1]*py[i];
        // Calcul de l'amplitude efficace:
        Puis = g1*g1 + g2*g2;
        iAmp = (Puis <= EPSILON) ? 0 : (1/(float)sqrt(2*Puis));
        //Normalisation des g, et prise en compte des distances:
        g[x->rev[hp]] += g1 * iAmp * x->dst_hp[x->rev[hp]] /pdist;
        g[x->rev[(hp+1)%x->N]]  +=  g2 * iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] /pdist;
    }
    // Calcul de la puissance global:
    Puis = 0;
    for(hp = 0; hp < x->N; hp++)
    Puis += (float)pow(g[hp]/x->dst_hp[x->rev[hp]], 2);
    // Affectation dans la dataspace avec normalisation :
    for(hp = 0; hp < x->N; hp++)
        x->G0[hp] = (float)(g[hp]/(sqrt(2*Puis)*x->r_c));
    return;
}

t_int *vbap_perform_signal(t_int *w){
    t_vbap *x = (t_vbap *)(w[1]);
    float  *input = (t_sample *)(w[2]);
    float  *xp = (t_sample *)(w[3]);     // x coord
    float  *yp = (t_sample *)(w[4]);     // y coord
    int     n = (int)(w[5+x->Nout]);
    int     phii;           // angle (de 0 à T_COS)
    float   in;
    int     hp;             // speaker
    int     i;              // sample index
    int     N = x->N;       // number of speakers,
    float   xl, yl, xtemp;  // coords
    float   g[Nmax];        // speaker gains
    float   phi;            // angle
    float   r;              // radius
    float   Puis, iAmp;     // gain power and amp
    float  *out[Nmax];      // out pointers
    for(hp = 0; hp < x->Nout; hp++)
        out[hp] = (t_sample *)(w[hp+5]);
    for(i = n-1; i >= 0; i--){
        in = input[i];
        xl = xp[i];         // variables, car elles vont être écrasées.
        yl = yp[i];
        if(x->x_cartesian){
            r = (float)sqrt(xl*xl + yl*yl);
            phi = (float)acos(xl/r);
            if(yl < 0)
                phi = 2*Pi - phi;
        }
        else{ // polar
            r = xl; // radius
            phi = vbap_angle_wrap(yl)*Pi/180; // angle (0 - 2pi)
            phii = (int)(yl*T_COS*I360) & (int)MASQUE;
            xtemp = xl * x->cosin[phii];
            phii = (int)(.25*T_COS - phii)&(int)MASQUE;
            yl = xl*x->cosin[phii];
            xl = xtemp;
            // maintenant xl = abscisse et yl = ordonnée.
            // Si r est négatif on modifie les coordonnées,
            if(r < 0){
                r = -r;
                phi = (phi < Pi) ? (phi + Pi) : (phi - Pi);
            }
        }
        for(hp = 0; hp < x->N; hp++) // remise à zéro des gains:
            g[hp] = 0;
        //Recherche des hp encadrant le point:
        for(hp = 0; hp < x->N-1; hp++){
            if(phi >= x->teta[x->rev[hp]] && phi < x->teta[x->rev[hp+1]])
                break;
        }
        // Calcul du gain:
        g[x->rev[hp]] = x->L[hp][0][0]*xl + x->L[hp][0][1]*yl;
        g[x->rev[(hp+1)%x->N]] = x->L[hp][1][0]*xl + x->L[hp][1][1]*yl;
    
        // Puissance du gain (source et hp sur le cercle) :
        Puis = (g[x->rev[hp]] * g[x->rev[hp]])
              + (g[x->rev[(hp+1)%x->N]]*g[x->rev[(hp+1)%x->N]]);
        iAmp = (Puis <= EPSILON) ? 0 : (1/(float)sqrt(2*Puis));
        if(r > x->r_c){  // Normalisation des g, et prise en compte des distances :
            g[x->rev[hp]] *=  iAmp * x->dst_hp[x->rev[hp]] / r;
            g[x->rev[(hp+1)%x->N]] *= iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] / r;
        }
        else{ // Calcul du gain (sur le disque central):
            g[x->rev[hp]] *=  iAmp * x->dst_hp[x->rev[hp]] * r/(x->r_c*x->r_c);
            g[x->rev[(hp+1)%x->N]] *=  iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] * r/(x->r_c*x->r_c);
            // ici le gain est proportionnelle à r/r_c dans le disque.
            // On mixe linéairement l'effet VBAP avec les gains pour la position centrale:
            for(hp = 0; hp < x->N; hp++)
                g[hp] += (1 - r/x->r_c) * x->G0[hp];
        }
        for(hp = N-1; hp >= 0; hp--)
            out[hp][i] = in * g[hp];
    }
    // Initialisation à zéro des sorties inutilisées
    for(hp = x->Nout-1; hp >= N; hp--){
        for(i = n-1; i >= 0; i--)
            out[hp][i] = 0;
    }
    return(w + x->Nout + 6);
}

void vbap_x(t_vbap *x, float xp){ // set gains from x
    float Puis, iAmp;
    int   hp;
    float yp  = x->y;
    float phi = x->phi;
    float r;
    if(!x->x_cartesian){ // Conversion polaires -> cartésiennes
        r  = xp;
        xp = (float)(r * cos(phi));
        yp = (float)(r * sin(phi));
        // maintenant xp = abscisse.
    }
    else{ // Convert cart -> polar
        r = (float)sqrt( xp*xp+yp*yp );
        phi = (float)acos( xp/r );
        if(yp < 0)
            phi = 2*Pi - phi;
    }
    x->r   = r;
    x->phi = phi;
    x->y   = yp;
    x->x   = xp;
    // Si r est négatif on modifie les coordonnées mais pas la dataspace,
    if(r < 0){
        r = -r;
        phi = (phi<Pi)?(phi+Pi):(phi-Pi);
    }
    // Remise à zéro de tout les gains cibles:
    for(hp = 0; hp < x->Nout; hp++)
        x->Gstop[hp] = 0;
    // Recherche des hp encadrant le point:
    for(hp = 0; hp < x->N-1; hp++){
        if(phi >= x->teta[x->rev[hp]] && phi < x->teta[x->rev[hp+1]])
            break;
    }
    // Calcul des gains :
    x->Gstop[x->rev[hp]]          = x->L[hp][0][0]*xp + x->L[hp][0][1]*yp;
    x->Gstop[x->rev[(hp+1)%x->N]] = x->L[hp][1][0]*xp + x->L[hp][1][1]*yp;
    // Puissance du gain (source et hp sur le cercle) :
    Puis = (x->Gstop[x->rev[hp]]*x->Gstop[x->rev[hp]])
        + (x->Gstop[x->rev[(hp+1)%x->N]]*x->Gstop[x->rev[(hp+1)%x->N]]);
    iAmp = (Puis <= EPSILON) ? 0 : (1/(float)sqrt(2*Puis));
    if(r > x->r_c){
        // Normalisation des g, et prise en compte des distances :
        x->Gstop[x->rev[hp]]           *=  iAmp * x->dst_hp[x->rev[ hp        ]] / r;
        x->Gstop[x->rev[(hp+1)%x->N]]  *=  iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] / r;
    }
    else{
        // Calcul du gain (sur le disque central):
        x->Gstop[x->rev[hp]] *=  iAmp * x->dst_hp[x->rev[hp]] * r/(x->r_c*x->r_c);
        x->Gstop[x->rev[(hp+1)%x->N]] *= iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] * r/(x->r_c*x->r_c);
        //ici le gain est proportionnelle à r/r_c dans le disque.
        //On mixe linéairement l'effet VBAP avec les gains pour la position centrale:
        for(hp = 0; hp < x->N; hp++)
            x->Gstop[hp] += (1 - r/x->r_c) * x->G0[hp];
    }
    return;
}

/* set parameters from y
void vbap_y(t_vbap *x, float yp){
    float Puis, iAmp;
    int hp;
    float xp = x->x;
    float r  = x->r;
    float phi;
    // Conversion polaire -> cartésienne,
    if(!x->x_cartesian){
        // ici yp = angle,
        phi = (float)(vbap_angle_wrap(yp)*Pi/180.);
        xp = (float)( r * cos( phi ) );
        yp = (float)( r * sin( phi ) );
        // maintenant yp = ordonnée,
    }
    // Conversion cartésienne -> polaire,
    else{
        r = (float)sqrt( xp*xp+yp*yp );
        phi = (float)acos( xp/r );
        if(yp < 0)
            phi = 2*Pi - phi;
    }
    x->y = yp;
    x->x = xp;
    x->r = r;
    x->phi = phi;
    // Si r est négatif on modifie les coordonnées mais pas la dataspace,
    if(r < 0){
        r = -r;
        phi = (phi<Pi)?(phi+Pi):(phi-Pi);
    }
    // Remise à zéro de tout les gains cibles:
    for(hp = 0; hp < x->Nout; hp++)
        x->Gstop[hp] = 0;
    // Recherche des hp encadrant le point:
    for(hp = 0; hp < x->N-1; hp++){
        if(phi >= x->teta[x->rev[hp]] && phi < x->teta[x->rev[hp+1]])
            break;
    }
    // Calcul des gains :
    x->Gstop[x->rev[hp]] = x->L[hp][0][0]*xp + x->L[hp][0][1]*yp;
    x->Gstop[x->rev[(hp+1)%x->N]] = x->L[hp][1][0]*xp + x->L[hp][1][1]*yp;
    // Puissance du gain (source et hp sur le cercle) :
    Puis = (x->Gstop[x->rev[hp]]*x->Gstop[x->rev[hp]])
        + (x->Gstop[x->rev[(hp+1)%x->N]]*x->Gstop[x->rev[(hp+1)%x->N]]);
    iAmp = (Puis <= EPSILON) ? 0 : (1/(float)sqrt(2*Puis));
    if(r > x->r_c){
        // Normalisation des g, et prise en compte des distances :
        x->Gstop[x->rev[hp]] *= iAmp * x->dst_hp[x->rev[hp]] / r;
        x->Gstop[x->rev[(hp+1)%x->N]] *= iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] / r;
    }
    else{
        // Calcul du gain (sur le disque central):
        x->Gstop[x->rev[hp]] *= iAmp * x->dst_hp[x->rev[hp]] * r/(x->r_c*x->r_c);
        x->Gstop[x->rev[(hp+1)%x->N]] *= iAmp * x->dst_hp[x->rev[(hp+1)%x->N]] * r/(x->r_c*x->r_c);
        // ici le gain est proportionnelle à r/r_c dans le disque.
        // On mixe linéairement l'effet VBAP avec les gains pour la position centrale:
        for(hp = 0; hp < x->N; hp++)
            x->Gstop[hp] += (1 - r/x->r_c) * x->G0[hp];
    }
    return;
}*/

// set speaker position with angle
void vbap_pos_a(t_vbap *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    for(int hp = 0; hp < x->N && hp < ac; hp++){ // set coords
        x->teta[hp] = (float)(vbap_angle_wrap(atom_getfloat(av+hp)) * Pi/180.);
        x->x_hp[hp] = (float)cos(x->teta[hp]);
        x->y_hp[hp] = (float)sin(x->teta[hp]);
    }
    vbap_set_matrix(x); // distance is set here
    return;
}

// set speaker position from angle and radius
void vbap_pos_polar(t_vbap *x, t_symbol *s, int ac, t_atom *av ){
    s = NULL;
    float rayon;
    for(int hp = 0; hp < x->N && 2*hp < ac; hp++){
        // Récupération des rayons et des angles.
        rayon = (float)(atom_getfloat(av+2*hp));
        if(2*hp + 1 < ac)
            x->teta[hp] = vbap_angle_wrap(atom_getfloat(av+2*hp + 1)) * Pi/180;
        // Affectations des nouvelles coordonnées:
        x->x_hp[hp] = rayon*(float)cos( x->teta[hp] );
        x->y_hp[hp] = rayon*(float)sin( x->teta[hp] );
    }
    // Modification des matrices relatives aux hp:
    vbap_set_matrix(x);
    // set gains
    if(x->x_cartesian)
        vbap_x(x, x->x);
    else
        vbap_x(x, x->r);
    return;
}

// position speaker with x & y coords
void vbap_pos( t_vbap *x, t_symbol *s, int ac, t_atom *av ){
    s = NULL;
    int hp;
    float xp, yp = 0, rayon, angle;
    // Récupération des coordonnées cartésiennes
    for(hp = 0; hp < x->N && 2*hp < ac; hp++){
        xp = (float)(atom_getfloat(av+2*hp));
        if(2*hp+1 < ac)
            yp = (float)(atom_getfloat(av+2*hp+1));
        else
            yp = x->y_hp[hp];
        // Conversion en polaire
        rayon = (float)sqrt(pow(xp, 2) + pow(yp, 2));
        if(rayon != 0){
            angle = (float)acos( (float)xp/rayon);
            if(yp < 0)
                angle = 2*Pi - angle;
        }
        else
            angle = 0;
        // Affectation des nouvelles coordonnées:
        x->teta[hp] = angle;
        x->x_hp[hp] = xp;
        x->y_hp[hp] = yp;
    }
    // Modification des matrices relatives aux hp:
    vbap_set_matrix(x);
    // Affectation des gains des hp
    if(x->x_cartesian)
        vbap_x( x, x->x);
    else
        vbap_x( x, x->r);
    return;
}

/*void vbap_n_speakers( t_vbap *x, float f){ // set n speakers
    int hp;
    int N = (int)f;
    if(x->Nout >= N && 2 < N){
        x->N = (int)N;
        // Modifications des angles des haut-parleurs et des rayons,
        for(hp = 0; hp < x->N; hp++){
            x->teta[hp] = (float)( Pi*( .5 + (1 - 2*hp )/(float)x->N) );
            if(x->teta[hp] < 0)
                x->teta[hp] += 2*Pi;
            x->x_hp[hp] = (float)cos(x->teta[hp]);
            x->y_hp[hp] = (float)sin(x->teta[hp]);
        }
        // Modification des matrices relatives aux hp:
        vbap_set_matrix(x);
    }
    else
        post("vbap~: Le nombre de haut-parleurs doit \210tre"
                   "inf\202rieur \205 %d, \n"
                   "  et sup\202rieur \205 3, ou \202gal.", x->Nout);
    // Affectation des gains
    if(x->x_cartesian)
        vbap_x(x, x->x);
    else
        vbap_x(x, x->r);
}

void vbap_central_radius( t_vbap *x, float r_c){
    if(r_c <= EPSILON){
        post("vbap~: central radius is null");
        return;
    }
    x->r_c = r_c;
    vbap_set_matrix(x); // Modification des matrices relatives aux hp:
    // Affectation des gains des hp
    if(x->x_cartesian)
        vbap_x(x, x->x);
    else
        vbap_x(x, x->r);
    return;
}*/

void vbap_info( t_vbap *x){
    int hp;
    post("Info [vbap~]:");
    if(x->x_cartesian)
        post("   cartesian");
    else
        post("   polar");
    post("   central radius = %f", x->r_c);
    post("   n speakers = %d", x->N);
    post("   position of speakers:");
    for(hp = 0; hp < x->N; hp++)
        post("      speaker (%d): %f / %f", hp+1, x->x_hp[hp], x->y_hp[hp]);
}

void vbap_dsp(t_vbap *x, t_signal **sp){
    switch(x->Nout){
        case 3:
        dsp_add(vbap_perform_signal, 8, x,
              sp[0]->s_vec,   //son d'entrŽe,
              sp[1]->s_vec,   //entrŽe de x,
              sp[2]->s_vec,   //entrŽe de y,
              sp[3]->s_vec,   //tous les sons de sortie, ...
              sp[4]->s_vec,   // .........
              sp[5]->s_vec,
              sp[0]->s_n);    //taille des blocs.
        break;
        case 4:
            dsp_add(vbap_perform_signal, 9, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[0]->s_n);
            break;
        case 5:
            dsp_add(vbap_perform_signal, 10, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[0]->s_n);
            break;
        case 6:
            dsp_add(vbap_perform_signal, 11, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[0]->s_n);
            break;
        case 7:
            dsp_add(vbap_perform_signal, 12, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[0]->s_n);
            break;
        case 8:
            dsp_add(vbap_perform_signal, 13, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[0]->s_n);
            break;
        case 9:
            dsp_add(vbap_perform_signal, 14, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[0]->s_n);
            break;
        case 10:
            dsp_add(vbap_perform_signal, 15, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[11]->s_vec, sp[0]->s_n);
            break;
        case 11:
            dsp_add(vbap_perform_signal, 16, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[0]->s_n);
            break;
        case 12:
            dsp_add(vbap_perform_signal, 17, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
              sp[0]->s_n);
            break;
        case 13:
            dsp_add(vbap_perform_signal, 18, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
              sp[15]->s_vec, sp[0]->s_n);
            break;
        case 14:
            dsp_add(vbap_perform_signal, 19, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
              sp[15]->s_vec, sp[16]->s_vec, sp[0]->s_n);
            break;
        case 15:
            dsp_add(vbap_perform_signal, 20, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
              sp[15]->s_vec, sp[16]->s_vec, sp[17]->s_vec,
              sp[0]->s_n);
            break;
        case 16:
            dsp_add(vbap_perform_signal, 21, x,
              sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
              sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec,
              sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec,
              sp[9]->s_vec, sp[10]->s_vec, sp[11]->s_vec,
              sp[12]->s_vec, sp[13]->s_vec, sp[14]->s_vec,
              sp[15]->s_vec, sp[16]->s_vec, sp[17]->s_vec,
              sp[18]->s_vec, sp[0]->s_n);
            break;
    }
}

void vbap_free(t_vbap *x){
    free(x->cosin);
    return;
}

void *vbap_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_vbap *x = (t_vbap *)pd_new(vbap_class);
    int i, hp;
    // default values
    x->N = Ndefaut;
    x->x_cartesian = 1;
    x->r_c = 0.5;
    x->y = Ydefaut;
    x->phi = Phidefaut;
    if(ac >= 1)
        x->N = atom_getint(av);
    if(x->N <= 2 || x->N > Nmax)
        x->N = Ndefaut;
    x->Nout = x->N;
    if(ac >= 2)
        x->r_c = (float)fabs(atom_getfloat(av+1));
    if(x->r_c <= EPSILON)
        x->r_c = (float)Rdisq;
    
    for(hp = x->N-1; hp >= 0; hp--)
        x->G[hp] = 0;
    for(hp = 0; hp < x->N; hp++){ // init angles and radius
        x->teta[hp] = (float)( Pi*( .5 + (1 - 2*hp )/(float)x->N) );
        if(x->teta[hp] < 0)
            x->teta[hp] += 2*Pi;
        x->x_hp[hp] = (float)cos( x->teta[hp]);
        x->y_hp[hp] = (float)sin( x->teta[hp]);
    }
    // Initialisation de tout les tableaux relatifs aux hp:
    vbap_set_matrix(x);
    // initialisation de x, X, Y, W et Pn par la mŽthode recoit x.
    if(x->x_cartesian)
        vbap_x(x, Xdefaut);
    else
        vbap_x(x, Rdefaut);
    // CrŽation du tableaux cosinus
    x->cosin = (float*)malloc(T_COS*sizeof(float));
    for(i = 0; i < T_COS; i++) // Remplissage du tableau cosinus
        x->cosin[i] = (float)cos(i*2*Pi/T_COS);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd,
        gensym("signal"), gensym("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd,
        gensym("signal"), gensym("signal"));
    for(i = 0; i < x->N; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    return(void *)x;
}

void vbap_tilde_setup(void){
    vbap_class = class_new( gensym("vbap~"), (t_newmethod) vbap_new,
        (t_method) vbap_free, sizeof(t_vbap), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(vbap_class, (t_method)vbap_dsp, gensym("dsp"), 0);
    class_addmethod(vbap_class, nullfn, gensym("signal"), 0);
//    class_addmethod(vbap_class, (t_method)vbap_n_speakers, gensym("n"), A_DEFFLOAT, 0);
//    class_addmethod(vbap_class, (t_method)vbap_x, gensym("x"), A_DEFFLOAT, 0);
//    class_addmethod(vbap_class, (t_method)vbap_y, gensym("y"), A_DEFFLOAT, 0);
    class_addmethod(vbap_class, (t_method)vbap_central_radius, gensym("r"), A_DEFFLOAT, 0);
    class_addmethod(vbap_class, (t_method)vbap_pos_a, gensym("pos_a"), A_GIMME, 0);
    class_addmethod(vbap_class, (t_method)vbap_pos_polar, gensym("pos_polar"), A_GIMME, 0);
    class_addmethod(vbap_class, (t_method)vbap_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(vbap_class, (t_method)vbap_info, gensym("info"), A_GIMME, 0);
}
