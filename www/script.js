const titulo = document.getElementById("titulo");
const boton = document.getElementById("boton-color");

let verde = false;

boton.addEventListener("click", () => {
    verde = !verde;
    titulo.style.color = verde ? "#6B8E23" : "";
});