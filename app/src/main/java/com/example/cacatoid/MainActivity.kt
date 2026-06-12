package com.example.cacatoid

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.res.ColorStateList
import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.viewModels
import androidx.core.content.ContextCompat
import com.example.cacatoid.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private val viewModel: SearchViewModel by viewModels()

    // Puzzle 20 is solved and has a tiny range — included so the search can be
    // tested end to end (it finds the key almost instantly).
    private val puzzles = listOf(20, 71, 72, 73)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setupPuzzleDropdown()
        setupButtons()
        observeViewModel()
    }

    private fun setupPuzzleDropdown() {
        val labels = puzzles.map { "Puzzle $it" }
        binding.puzzleDropdown.setAdapter(
            ArrayAdapter(this, android.R.layout.simple_list_item_1, labels)
        )
        val initial = puzzles.indexOf(viewModel.selectedPuzzle).coerceAtLeast(0)
        binding.puzzleDropdown.setText(labels[initial], false)
        binding.puzzleDropdown.setOnItemClickListener { _, _, pos, _ ->
            viewModel.selectedPuzzle = puzzles[pos]
        }
    }

    private fun setupButtons() {
        binding.startButton.setOnClickListener { viewModel.start() }
        binding.stopButton.setOnClickListener { viewModel.stop() }
        binding.copyButton.setOnClickListener { copyKey() }
    }

    private fun observeViewModel() {
        viewModel.running.observe(this) { running ->
            binding.startButton.isEnabled = !running
            binding.stopButton.isEnabled = running
            binding.puzzleInputLayout.isEnabled = !running
            binding.progress.visibility = if (running) View.VISIBLE else View.GONE
            binding.statusText.text = getString(
                if (running) R.string.status_running else R.string.status_stopped
            )
            tintStatusDot(if (running) R.color.success else R.color.dot_idle)
        }

        viewModel.stats.observe(this) { s ->
            binding.currentKeyText.text =
                if (s.currentKeyHex.isEmpty()) getString(R.string.dash) else s.currentKeyHex
            binding.speedValue.text = formatNumber(s.keysPerSec)
            binding.totalValue.text = formatNumber(s.totalChecked)
        }

        viewModel.found.observe(this) { found ->
            if (found == null) {
                binding.resultPanel.visibility = View.GONE
            } else {
                binding.resultPanel.visibility = View.VISIBLE
                binding.resultText.text = buildString {
                    append("Puzzle:   ${found.puzzle}\n")
                    append("Address:  ${found.address}\n\n")
                    append("Priv hex: ${found.privKeyHex}\n\n")
                    append("WIF:      ${found.wif}")
                }
                binding.statusText.text = getString(R.string.match_found)
                tintStatusDot(R.color.success)
                Toast.makeText(this, R.string.match_found, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun tintStatusDot(colorRes: Int) {
        binding.statusDot.backgroundTintList =
            ColorStateList.valueOf(ContextCompat.getColor(this, colorRes))
    }

    private fun copyKey() {
        val found = viewModel.found.value ?: return
        val clip = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clip.setPrimaryClip(ClipData.newPlainText("private key", found.privKeyHex))
        Toast.makeText(this, "Private key copied", Toast.LENGTH_SHORT).show()
    }

    private fun formatNumber(n: Long): String = "%,d".format(n)
}
