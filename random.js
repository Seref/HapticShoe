let s = "";

for(let i = 0; i < 255; i++) {
    s += (Math.random()>0.5 ? 1 : -1)*Math.random().toFixed(4)+"f,";
}

console.log(s);