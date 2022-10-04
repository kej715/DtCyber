class ContainerStack {

  constructor() {
    this.stack = [];
  }

  push(element) {
    const me = this;
    let id = element.getAttribute("id");
    let len = this.stack.unshift({
      id: id,
      element: element
    });
    element.style.zIndex = len + 5;
    element.addEventListener("click", evt => {
      me.promote(element);
    });
  }

  promote(container) {
    let id = container.getAttribute("id");
    for (let i = 0; i < this.stack.length; i++) {
      if (this.stack[i].id === id) {
        let items = this.stack.splice(i, 1);
        this.stack.unshift(items[0]);
        break;
      }
    }
    let zindex = this.stack.length + 5;
    for (let i = 0; i < this.stack.length; i++) {
      this.stack[i].element.style.zIndex = zindex--;
    }
  }
}

class DraggableManager {

  constructor() {
  }

  activate(draggable) {

    draggable.onmousedown = e => {

      e.preventDefault();
      let lastX = e.clientX;
      let lastY = e.clientY;

      document.onmouseup = () => {
        document.onmouseup = null;
        document.onmousemove = null;
      };

      document.onmousemove = e => {
        e.preventDefault();
        let currentX = e.clientX;
        let currentY = e.clientY;
        let deltaX = lastX - currentX;
        let deltaY = lastY - currentY;
        lastX = currentX;
        lastY = currentY;
        draggable.style.top = (draggable.offsetTop - deltaY) + "px";
        draggable.style.left = (draggable.offsetLeft - deltaX) + "px";
      };
    };
  }
}
