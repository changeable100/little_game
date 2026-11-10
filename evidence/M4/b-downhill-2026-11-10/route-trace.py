"""Make a host-only diagnostic copy; no changes to replay conditions."""
from pathlib import Path
import re

src = Path('src/roll3_selftest.c').read_text(encoding='utf-8')
src = '#include <stdio.h>\n' + src
src = src.replace('return dx*dx+dy*dy+dz*dz<=eps*eps;',
                  'if(boundaries==1) printf("seam_endpoint_offset_mm %.6f\\n",'
                  '1000*sqrtf(dx*dx+dy*dy+dz*dz));\n'
                  '    return dx*dx+dy*dy+dz*dz<=eps*eps;')
src = src.replace('++next;du=route[next].x-p.x;',
                  'printf("turn=%u elapsed_ms=%lu u=%.4f v=%.4f speed=%.4f checkpoint=%lu\\n",'
                  'next+1,(unsigned long)g.s.elapsed_ms,p.x,p.y,sqrtf(norm2(v)),(unsigned long)g.s.checkpoint);\n'
                  '            ++next;du=route[next].x-p.x;')
src = re.sub(r'\+\+seams;}', '++seams;printf("cross=%u elapsed_ms=%lu u=%.4f v=%.4f z=%.4f\\\\n",'
             'seams,(unsigned long)g.s.elapsed_ms,p.x,p.y,p.z);}', src)
src = src.replace('CHECK(seams==6&&next==2&&g.s.state==R3_WON',
                  'printf("finish elapsed_ms=%lu state=%d seams=%u checkpoint=%lu falls=%lu\\n",'
                  '(unsigned long)g.s.elapsed_ms,(int)g.s.state,seams,(unsigned long)g.s.checkpoint,(unsigned long)g.s.falls);\n'
                  '    CHECK(seams==6&&next==2&&g.s.state==R3_WON')
src += '''
int main(void) {
    const char *names[]={"S-R1","R1-C1","C1-R2","R2-C2","C2-R3","R3-G"};
    for(unsigned i=0;i<6;++i){
        printf("seam %s: offsets of each top edge from each shared endpoint\\n",names[i]);
        if(!deck_seam(i))return 1;
    }
    if(!demo_route())return 1;
    puts("TC-513 instrumented replay PASS");
    return 0;
}
'''
Path('build/m1_trace.c').write_text(src, encoding='utf-8')
