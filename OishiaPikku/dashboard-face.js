// OLED face geometry mirrors drawMascotFace/drawEyes in learning.ino.
// Render locally from the current mood; no image download or extra API traffic.
function drawOishiaFace(canvas, mood, detailed = true) {
  const ctx = canvas.getContext('2d');
  const constrain = (value,low,high) => Math.max(low,Math.min(high,value));
  const pupilX = 0, pupilY = 0, tiltDirection = 0;
  const color = c => c ? '#fff' : '#000';
  const rect = (x,y,w,h,c) => {ctx.fillStyle=color(c);ctx.fillRect(x,y,w,h);};
  const circle = (x,y,r,c,fill) => {ctx.beginPath();ctx.arc(x+.5,y+.5,r,0,Math.PI*2);ctx.fillStyle=ctx.strokeStyle=color(c);fill?ctx.fill():ctx.stroke();};
  const rounded = (x,y,w,h,r,c,fill) => {ctx.beginPath();ctx.roundRect(x+.5,y+.5,w-1,h-1,r);ctx.fillStyle=ctx.strokeStyle=color(c);fill?ctx.fill():ctx.stroke();};
  const display = {
    drawPixel:(x,y,c)=>rect(x,y,1,1,c), fillRect:rect,
    drawLine:(x,y,xx,yy,c)=>{ // Integer Bresenham, like Adafruit GFX.
      let dx=Math.abs(xx-x),sx=x<xx?1:-1,dy=-Math.abs(yy-y),sy=y<yy?1:-1,err=dx+dy;
      for(;;){rect(x,y,1,1,c);if(x===xx&&y===yy)break;const e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
    },
    drawCircle:(x,y,r,c)=>circle(x,y,r,c,false),fillCircle:(x,y,r,c)=>circle(x,y,r,c,true),
    drawRoundRect:(x,y,w,h,r,c)=>rounded(x,y,w,h,r,c,false),fillRoundRect:(x,y,w,h,r,c)=>rounded(x,y,w,h,r,c,true),
    fillTriangle:(x,y,xx,yy,xxx,yyy,c)=>{ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(xx,yy);ctx.lineTo(xxx,yyy);ctx.closePath();ctx.fillStyle=color(c);ctx.fill();}
  };
  ctx.clearRect(0,0,128,64);rect(0,0,128,64,0);
  const style = ({HAPPY:'HAPPY',GREETING:'HAPPY',LOVE:'LOVE',SLEEP:'SLEEP',THINKING:'THINKING',CURIOUS:'CURIOUS',EXCITED:'SURPRISED',SAD:'SAD',ANGRY:'ANGRY'})[mood] || 'NORMAL';
  if(['LOVE','EXCITED','GREETING'].includes(mood)){drawHeart(8,7,11);drawHeart(110,8,9);}
  else if(['THINKING','CURIOUS'].includes(mood))drawSparkles(0);
  else if(mood==='SAD')drawWorryDrops(0,0);
  else if(mood==='ANGRY')drawAngrySteam(0);
  if(detailed)drawMascotFace(style,mood,0);
  else{drawEyes(style,0);drawMouth(mood,0);drawCheeks(0);}
  if(mood==='SLEEP'){ctx.fillStyle='#fff';ctx.font='8px monospace';ctx.fillText('z',101,25);}
  canvas.dataset.mood=mood;canvas.dataset.detail=String(detailed);
  canvas.setAttribute('aria-label','Oishia: '+mood.toLowerCase());
function drawMascotFace(style, state, yOffset) {
  const y = yOffset;

  // A bold bear-cat silhouette stays readable on a real 128x64 OLED. Body,
  // tail, ears and paws are drawn first so the face panel remains uncluttered.
  display.fillCircle(98, 43 + y, 9, 1);
  display.fillCircle(102, 39 + y, 4, 0);
  display.fillRoundRect(39, 38 + y, 50, 15, 7, 1);
  display.fillCircle(45, 47 + y, 8, 1);
  display.fillCircle(83, 47 + y, 8, 1);
  display.fillCircle(35, 10 + y, 12, 1);
  display.fillCircle(93, 10 + y, 12, 1);
  display.fillCircle(35, 10 + y, 5, 0);
  display.fillCircle(93, 10 + y, 5, 0);
  display.fillRoundRect(22, 4 + y, 84, 46, 20, 1);
  display.fillTriangle(58, 5 + y, 63, 0 + y, 66, 6 + y, 1);
  display.fillTriangle(64, 6 + y, 70, 1 + y, 72, 8 + y, 1);
  display.fillRoundRect(29, 10 + y, 70, 34, 15, 0);

  const leftEye = 48;
  const rightEye = 80;
  const eyeY = 24 + y;
  if (style == 'HAPPY') {
    display.drawLine(leftEye - 7, eyeY + 2, leftEye, eyeY - 3, 1);
    display.drawLine(leftEye, eyeY - 3, leftEye + 7, eyeY + 2, 1);
    display.drawLine(rightEye - 7, eyeY + 2, rightEye, eyeY - 3, 1);
    display.drawLine(rightEye, eyeY - 3, rightEye + 7, eyeY + 2, 1);
  } else if (style == 'SLEEP') {
    display.fillRoundRect(leftEye - 7, eyeY, 14, 2, 1, 1);
    display.fillRoundRect(rightEye - 7, eyeY, 14, 2, 1, 1);
  } else if (style == 'LOVE') {
    drawHeart(leftEye - 6, eyeY - 6, 12);
    drawHeart(rightEye - 6, eyeY - 6, 12);
  } else {
    let leftRadius = style == 'CURIOUS' ? 8 : 7;
    let rightRadius = style == 'CURIOUS' ? 5 : 7;
    display.fillCircle(leftEye, eyeY, leftRadius, 1);
    display.fillCircle(rightEye, eyeY, rightRadius, 1);
    let lookX = constrain(Math.trunc(pupilX) + Math.trunc(tiltDirection), -1, 1);
    let lookY = style == 'THINKING' ? -1 : constrain(Math.trunc(pupilY), -1, 1);
    let pupilRadius = style == 'SURPRISED' ? 2 : 3;
    display.fillCircle(leftEye + lookX * 2, eyeY + lookY * 2, pupilRadius, 0);
    display.fillCircle(rightEye + lookX * 2, eyeY + lookY * 2, pupilRadius, 0);
    display.drawPixel(leftEye + lookX * 2 - 1, eyeY + lookY * 2 - 1, 1);
    display.drawPixel(rightEye + lookX * 2 - 1, eyeY + lookY * 2 - 1, 1);
    if (style == 'SAD') {
      display.drawLine(leftEye - 7, eyeY - 9, leftEye + 4, eyeY - 6, 1);
      display.drawLine(rightEye - 4, eyeY - 6, rightEye + 7, eyeY - 9, 1);
    } else if (style == 'ANGRY') {
      display.drawLine(leftEye - 7, eyeY - 7, leftEye + 4, eyeY - 10, 1);
      display.drawLine(rightEye - 4, eyeY - 10, rightEye + 7, eyeY - 7, 1);
    }
  }

  // Tiny muzzle and cheeks mirror the dashboard mascot without fine detail.
  display.fillTriangle(61, 32 + y, 67, 32 + y, 64, 35 + y, 1);
  display.drawLine(64, 35 + y, 64, 37 + y, 1);
  if (state == 'SAD') {
    display.drawLine(58, 41 + y, 64, 37 + y, 1);
    display.drawLine(64, 37 + y, 70, 41 + y, 1);
  } else if (state == 'ANGRY') {
    display.drawLine(58, 39 + y, 70, 39 + y, 1);
  } else if (state == 'EXCITED') {
    display.drawCircle(64, 40 + y, 3, 1);
  } else {
    display.drawLine(57, 37 + y, 61, 40 + y, 1);
    display.drawLine(61, 40 + y, 64, 37 + y, 1);
    display.drawLine(64, 37 + y, 67, 40 + y, 1);
    display.drawLine(67, 40 + y, 71, 37 + y, 1);
  }
  display.drawPixel(35, 35 + y, 1);
  display.drawPixel(38, 36 + y, 1);
  display.drawPixel(90, 35 + y, 1);
  display.drawPixel(93, 36 + y, 1);

  // Paws and a small dark heart identify Oishia even when the face is asleep.
  display.drawLine(43, 47 + y, 46, 50 + y, 0);
  display.drawLine(85, 47 + y, 82, 50 + y, 0);
  display.fillCircle(61, 47 + y, 2, 0);
  display.fillCircle(67, 47 + y, 2, 0);
  display.fillTriangle(59, 47 + y, 69, 47 + y, 64, 52 + y, 0);
}

function drawEyes(style, yOffset) {
  let y = 16 + yOffset;
  const left = 29;
  const right = 75;
  if (style == 'NORMAL') {
    let lookX = tiltDirection == 0 ? pupilX : tiltDirection;
    drawOpenEye(left, y, 24, 24, lookX, pupilY);
    drawOpenEye(right, y, 24, 24, lookX, pupilY);
  } else if (style == 'HAPPY') {
    // Thick, rounded smile arches stay readable on the tiny OLED.
    for (let eye = 0; eye < 2; ++eye) {
      let x = eye == 0 ? left : right;
      display.fillRoundRect(x, y + 6, 24, 20, 11, 1);
      display.fillRoundRect(x + 3, y + 10, 18, 18, 8, 0);
      display.fillRect(x, y + 17, 24, 12, 0);
    }
  } else if (style == 'LOVE') {
    drawHeart(left + 2, y + 3, 20);
    drawHeart(right + 2, y + 3, 20);
  } else if (style == 'SLEEP') {
    display.fillRoundRect(left + 2, y + 13, 20, 3, 1, 1);
    display.fillRoundRect(right + 2, y + 13, 20, 3, 1, 1);
  } else if (style == 'CURIOUS') {
    drawOpenEye(left + 2, y + 3, 20, 21, 1, -1);
    drawOpenEye(right - 1, y - 2, 26, 27, 1, -1);
  } else if (style == 'SURPRISED') {
    display.fillCircle(left + 12, y + 12, 12, 1);
    display.fillCircle(right + 12, y + 12, 12, 1);
    display.fillCircle(left + 12, y + 13, 4, 0);
    display.fillCircle(right + 12, y + 13, 4, 0);
  } else if (style == 'SAD') {
    display.drawLine(left + 1, y + 4, left + 22, y, 1);
    display.drawLine(right + 1, y, right + 22, y + 4, 1);
    drawOpenEye(left + 1, y + 5, 22, 19, 0, 1);
    drawOpenEye(right + 1, y + 5, 22, 19, 0, 1);
  } else if (style == 'ANGRY') {
    // Eyebrows point inward and the smaller eyes give Oishia a comic grumpy look.
    display.drawLine(left + 1, y + 2, left + 22, y + 7, 1);
    display.drawLine(right + 1, y + 7, right + 22, y + 2, 1);
    drawOpenEye(left + 1, y + 9, 22, 15, 0, 1);
    drawOpenEye(right + 1, y + 9, 22, 15, 0, 1);
  } else { // 'THINKING'
    drawOpenEye(left, y, 24, 24, 0, -1);
    drawOpenEye(right, y, 24, 24, 0, -1);
    display.drawLine(left + 3, y - 3, left + 19, y - 5, 1);
  }
}

function drawOpenEye(x, y, w, h, lookX, lookY) {
  display.fillRoundRect(x, y, w, h, Math.trunc(w / 3), 1);
  let pupilRadius = Math.trunc(h / 5);
  let pupilXPos = x + Math.trunc(w / 2) + lookX * 2;
  let pupilYPos = y + Math.trunc(h / 2) + lookY * 2;
  display.fillCircle(pupilXPos, pupilYPos, pupilRadius, 0);
  display.drawPixel(pupilXPos - 1, pupilYPos - 1, 1);
}

function drawMouth(state, yOffset) {
  let y = 42 + yOffset;
  if (state == 'LOVE') drawHeart(59, y - 3, 11);
  else if (state == 'SLEEP') display.drawLine(59, y, 69, y, 1);
  else if (state == 'EXCITED') {
    display.fillCircle(64, y + 2, 6, 1);
    display.fillCircle(64, y + 1, 3, 0);
  } else if (state == 'SAD') {
    display.drawLine(58, y + 4, 64, y, 1);
    display.drawLine(64, y, 70, y + 4, 1);
  } else if (state == 'ANGRY') {
    display.drawLine(56, y + 3, 61, y + 1, 1);
    display.drawLine(61, y + 1, 66, y + 4, 1);
    display.drawLine(66, y + 4, 72, y + 1, 1);
  } else if (state == 'THINKING') {
    display.drawLine(59, y + 3, 68, y + 3, 1);
    display.drawPixel(70, y + 2, 1);
  } else if (state == 'CURIOUS') display.drawCircle(64, y + 2, 4, 1);
  else {
    display.drawLine(59, y + 1, 61, y + 4, 1);
    display.drawLine(61, y + 4, 66, y + 4, 1);
    display.drawLine(66, y + 4, 68, y + 1, 1);
  }
}

function drawCheeks(yOffset) {
  let y = 39 + yOffset;
  display.fillRoundRect(20, y, 7, 3, 1, 1);
  display.fillRoundRect(101, y, 7, 3, 1, 1);
}

function drawHeart(x, y, size) {
  let radius = Math.trunc(size / 4);
  let half = Math.trunc(size / 2);
  display.fillCircle(x + radius + 1, y + radius + 1, radius, 1);
  display.fillCircle(x + size - radius - 1, y + radius + 1, radius, 1);
  display.fillTriangle(x, y + radius + 1, x + size - 1, y + radius + 1, x + half, y + size - 1, 1);
}

function drawSparkles(frame) {
  display.drawPixel(18, 15, 1);
  display.drawPixel(17, 16, 1);
  display.drawPixel(19, 16, 1);
  display.drawPixel(18, 17, 1);
  if (frame & 1) {
    display.drawPixel(109, 29, 1);
    display.drawPixel(108, 30, 1);
    display.drawPixel(110, 30, 1);
    display.drawPixel(109, 31, 1);
  }
}

function drawArrivalSparkles(frame) {
  // A little burst around Oishia makes a PIR arrival obvious at a glance.
  display.drawLine(4, 25, 9, 25, 1);
  display.drawLine(6, 23, 6, 27, 1);
  display.drawPixel(120, 31, 1);
  display.drawPixel(119, 30, 1);
  display.drawPixel(121, 30, 1);
  display.drawPixel(120, 29, 1);
  if (frame & 1) {
    display.drawPixel(16, 44, 1);
    display.drawPixel(17, 43, 1);
    display.drawPixel(111, 45, 1);
    display.drawPixel(112, 44, 1);
  }
}

function drawWorryDrops(yOffset, frame) {
  let dropY = 36 + yOffset + frame;
  display.drawPixel(25, dropY, 1);
  display.drawPixel(24, dropY + 1, 1);
  display.drawPixel(25, dropY + 2, 1);
  display.drawPixel(103, dropY + (frame & 1), 1);
  display.drawPixel(104, dropY + 1 + (frame & 1), 1);
}

function drawAngrySteam(frame) {
  let leftX = 18 + (frame & 1);
  let rightX = 107 - (frame & 1);
  display.drawPixel(leftX, 12, 1);
  display.drawPixel(leftX + 1, 10, 1);
  display.drawPixel(leftX, 8, 1);
  display.drawPixel(rightX, 12, 1);
  display.drawPixel(rightX - 1, 10, 1);
  display.drawPixel(rightX, 8, 1);
}

}
