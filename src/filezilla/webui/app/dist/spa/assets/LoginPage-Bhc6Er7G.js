import{d as _,z as u,ba as v,aR as y,o as w,a as b,w as a,Q,b as e,aT as p,aI as L,aU as m,m as h,aY as I,aH as x,aZ as V,a_ as g,p as C,i as P,h as S,_ as k}from"./index-CLWElCvo.js";import{Q as U}from"./QForm-D1L5Iqw0.js";const B=l=>(C("data-v-7e19eab2"),l=l(),P(),l),A=B(()=>S("div",{class:"text-h6"},"Welcome",-1)),q=_({__name:"LoginPage",setup(l){const n=u(""),i=u(""),f=v(),d=y(),r=u(!1),c=async()=>{try{await g.login(n.value,i.value);const o=sessionStorage.getItem("redirectAfterLogin")||"/";sessionStorage.removeItem("redirectAfterLogin"),f.replace(o),d.notify({type:"positive",message:"Login successful"})}catch(o){if(o instanceof g.errors.Login)d.notify({type:"negative",message:"Login failed. Please check your credentials and try again."});else throw o}};return(o,s)=>(w(),b(Q,{class:"login-page"},{default:a(()=>[e(V,{class:"login-card"},{default:a(()=>[e(U,{"on-submit":c},{default:a(()=>[e(p,{class:"bg-primary text-white"},{default:a(()=>[A]),_:1}),e(L),e(p,null,{default:a(()=>[e(m,{outlined:"",modelValue:n.value,"onUpdate:modelValue":s[0]||(s[0]=t=>n.value=t),label:"Username",type:"text",autocomplete:"username",rules:[t=>!!t||"Username is required"]},null,8,["modelValue","rules"]),e(m,{outlined:"",modelValue:i.value,"onUpdate:modelValue":s[2]||(s[2]=t=>i.value=t),label:"Password",type:r.value?"text":"password",autocomplete:"new-password"},{append:a(()=>[e(h,{name:r.value?"visibility_off":"visibility",class:"cursor-pointer",onClick:s[1]||(s[1]=t=>r.value=!r.value)},null,8,["name"])]),_:1},8,["modelValue","type"])]),_:1}),e(I,{align:"right"},{default:a(()=>[e(x,{flat:"",type:"submit",label:"Login",color:"primary",onClick:c})]),_:1})]),_:1})]),_:1})]),_:1}))}}),$=k(q,[["__scopeId","data-v-7e19eab2"],["__file","LoginPage.vue"]]);export{$ as default};