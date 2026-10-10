// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
package org.stratoemu.strato.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.util.AttributeSet
import android.view.View
import com.google.android.material.color.MaterialColors
import kotlin.math.max

/**
 * Original Android implementation inspired by Citron's *concept* of a
 * frame-time history graph. Displays host presentation frame-times supplied
 * by Synolsis; it does not invent guest FPS or copy Citron Qt overlay code.
 */
class SynolsisPerformanceGraphView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs) {
    private val samples = ArrayDeque<Float>(96)
    private val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = MaterialColors.getColor(this@SynolsisPerformanceGraphView,
            com.google.android.material.R.attr.colorPrimary)
        strokeWidth = 2f * resources.displayMetrics.density
        style = Paint.Style.STROKE
    }
    private val guidePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(90, 200, 200, 200)
        strokeWidth = 1f * resources.displayMetrics.density
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 11f * resources.displayMetrics.scaledDensity
    }
    fun addSample(frameTimeMs: Float) {
        if (!frameTimeMs.isFinite() || frameTimeMs <= 0f || frameTimeMs > 1000f) return
        if (samples.size == 96) samples.removeFirst()
        samples.addLast(frameTimeMs)
        invalidate()
    }
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (samples.isEmpty()) return
        val density = resources.displayMetrics.density
        val left = 9f * density
        val right = width - 9f * density
        val top = 18f * density
        val bottom = height - 9f * density
        if (right <= left || bottom <= top) return
        val ceiling = max(50f, (samples.maxOrNull() ?: 50f) * 1.1f)
        val baseline = bottom - (16.67f / ceiling) * (bottom - top)
        canvas.drawLine(left, baseline, right, baseline, guidePaint)
        val points = samples.toList()
        val path = Path()
        points.forEachIndexed { index, sample ->
            val x = left + (right - left) * (index.toFloat() / max(1, points.size - 1))
            val y = bottom - sample.coerceIn(0f, ceiling) / ceiling * (bottom - top)
            if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)
        }
        canvas.drawPath(path, linePaint)
        val average = points.average()
        canvas.drawText(
            String.format(java.util.Locale.US, "min %.1f / avg %.1f / max %.1f ms",
                points.minOrNull() ?: 0f, average, points.maxOrNull() ?: 0f),
            left, 13f * density, labelPaint
        )
    }
}
