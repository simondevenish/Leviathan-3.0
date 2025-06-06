/* File generated with Shader Minifier 1.2
 * http://www.ctrl-alt-test.fr
 */
#ifndef POST_INL_
# define POST_INL_
# define VAR_I "v"
# define VAR_O "f"

const char *post_frag =
 "#version 130\n"
 "uniform sampler2D f;"
 "out vec4 v;"
 "float t(float f)"
 "{"
   "return fract(sin(dot(f,12.9898))*43758.5453);"
 "}"
 "void main()"
 "{"
   "v=vec4(0);"
   "for(int s=0;s<25;s++)"
     "{"
       "vec2 i=gl_FragCoord.xy/vec2(1280,720);"
       "float d=t(float(s+dot(i,i))),o=t(float(1-s+dot(i,i)));"
       "vec2 m=.01*(-1.+2.*vec2(d,o));"
       "v+=vec4(textureLod(f,i,(.3+.7*d)*texture(f,(1.+m)*i).w*8).xyz,1);"
     "}"
   "v/=vec4(25);"
 "}";

#endif // POST_INL_
